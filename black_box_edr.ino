#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include <time.h>
#include <Wire.h>
#include <SD.h>
#include <SPI.h>

// -------- MOTOR PINS --------
const int in1 = 26;
const int in2 = 25;
const int in3 = 33;
const int in4 = 32;

// -------- PINS --------
#define TEMP_PIN   34
#define VIB_PIN    27
#define ALERT_PIN  14
#define SD_CS_PIN   5

// -------- MPU6050 --------
#define MPU_ADDR   0x68
float accX, accY, accZ;
float gForce = 0;

// -------- WIFI --------
const char* ssid     = "YOUR HOTSPOT NAME";
const char* password = "YOUR HOTSPOT PASSWORD";

// -------- THINGSBOARD --------
const char* mqtt_server = "thingsboard.cloud";
const char* token       = "YOUR TOKEN";

WiFiClient   espClient;
PubSubClient client(espClient);
WebServer    server(80);

// -------- DATA --------
float  latestTemp   = 0;
int    latestVib    = 0;
String latestTime   = "";
String systemStatus = "NORMAL";

// -------- CRASH --------
int  vibrationCount  = 0;
int  peakVib         = 0;
bool crashDetected   = false;
unsigned long crashStartTime = 0;
bool restartPending  = false;

// -------- INTERVALS --------
unsigned long lastSendTime   = 0;
unsigned long lastSDWrite    = 0;
const unsigned long SEND_INTERVAL   = 3000;
const unsigned long SD_WRITE_INTERVAL = 1000;

// -------- MOTOR STATE --------
// Separate motor logic from server.send()
// to avoid timeout issues
String pendingMotorCmd = "";

// -------- SD LOGGING --------
String logFileName = "";
bool   sdReady     = false;

// -------- RING BUFFER --------
#define RING_SIZE 180
struct LogEntry {
  char   time[10];
  float  temp;
  int    vib;
  float  gforce;
  char   status[20];
};
LogEntry ringBuffer[RING_SIZE];
int ringHead  = 0;
int ringCount = 0;

// ======================================================
// MPU6050
// ======================================================
void mpu_init() {
  Wire.begin(21, 22);
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0x00);
  Wire.endTransmission(true);
  delay(100);

  // Verify MPU is responding
  Wire.beginTransmission(MPU_ADDR);
  byte error = Wire.endTransmission();
  if (error == 0) {
    Serial.println("✅ MPU6050 found");
  } else {
    Serial.println("❌ MPU6050 NOT found — check wiring");
  }
}

void mpu_read() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 6, true);

  if (Wire.available() < 6) {
    gForce = 0;
    return;
  }

  int16_t ax = (Wire.read() << 8) | Wire.read();
  int16_t ay = (Wire.read() << 8) | Wire.read();
  int16_t az = (Wire.read() << 8) | Wire.read();

  accX   = ax / 16384.0;
  accY   = ay / 16384.0;
  accZ   = az / 16384.0;
  gForce = sqrt(accX*accX + accY*accY + accZ*accZ);
}

// ======================================================
// TIME
// ======================================================
String getTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "00:00:00";
  char buf[10];
  sprintf(buf, "%02d:%02d:%02d",
          timeinfo.tm_hour,
          timeinfo.tm_min,
          timeinfo.tm_sec);
  return String(buf);
}

long getEpochMillis() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return millis();
  return (long)(mktime(&timeinfo)) * 1000L;
}

// ======================================================
// SD CARD
// ======================================================
void sd_init() {
  // Release SPI bus before init
  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, HIGH);
  delay(100);

  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("❌ SD Card FAILED — continuing without SD");
    sdReady = false;
    return;
  }
  sdReady = true;
  Serial.println("✅ SD Card ready");

  int fileNum = 1;
  while (fileNum < 999) {
    char fname[20];
    sprintf(fname, "/log_%03d.txt", fileNum);
    if (!SD.exists(fname)) {
      logFileName = String(fname);
      break;
    }
    fileNum++;
  }

  File f = SD.open(logFileName, FILE_WRITE);
  if (f) {
    f.println("TIME,TEMP,VIB,GFORCE,STATUS");
    f.close();
    Serial.print("📄 Log: ");
    Serial.println(logFileName);
  }
}

void sd_writeLine(const char* time, float temp,
                  int vib, float gforce, const char* status) {
  if (!sdReady) return;
  File f = SD.open(logFileName, FILE_APPEND);
  if (f) {
    f.print(time);      f.print(",");
    f.print(temp, 2);   f.print(",");
    f.print(vib);       f.print(",");
    f.print(gforce, 3); f.print(",");
    f.println(status);
    f.close();
  }
}

// ======================================================
// RING BUFFER
// ======================================================
void ring_push(String time, float temp, int vib,
               float gforce, String status) {
  int idx = (ringHead + ringCount) % RING_SIZE;
  if (ringCount < RING_SIZE) {
    ringCount++;
  } else {
    ringHead = (ringHead + 1) % RING_SIZE;
  }
  strncpy(ringBuffer[idx].time,   time.c_str(),   9);
  strncpy(ringBuffer[idx].status, status.c_str(), 19);
  ringBuffer[idx].temp   = temp;
  ringBuffer[idx].vib    = vib;
  ringBuffer[idx].gforce = gforce;
}

void ring_dump_to_sd() {
  if (!sdReady) return;
  File f = SD.open(logFileName, FILE_APPEND);
  if (!f) return;
  f.println("=== PRE-CRASH LOG (last 3 min) ===");
  for (int i = 0; i < ringCount; i++) {
    int idx = (ringHead + i) % RING_SIZE;
    f.print(ringBuffer[idx].time);      f.print(",");
    f.print(ringBuffer[idx].temp, 2);   f.print(",");
    f.print(ringBuffer[idx].vib);       f.print(",");
    f.print(ringBuffer[idx].gforce, 3); f.print(",");
    f.println(ringBuffer[idx].status);
  }
  f.println("=== CRASH EVENT ===");
  f.close();
  Serial.println("💾 Pre-crash dumped to SD");
}

// ======================================================
// MOTOR CONTROL — no server.send() here
// ======================================================
void setMotor(int a, int b, int c, int d) {
  digitalWrite(in1, a);
  digitalWrite(in2, b);
  digitalWrite(in3, c);
  digitalWrite(in4, d);
}

void doForward()  { setMotor(LOW,  HIGH, LOW,  HIGH); }
void doBackward() { setMotor(HIGH, LOW,  HIGH, LOW);  }
void doLeft()     { setMotor(HIGH, LOW,  LOW,  HIGH); }
void doRight()    { setMotor(LOW,  HIGH, HIGH, LOW);  }
void doStop()     { setMotor(LOW,  LOW,  LOW,  LOW);  }

// ======================================================
// WEB HANDLERS — only server.send(), no motor logic
// ======================================================
void handleForward()  { doForward();  server.send(200,"text/plain","F"); }
void handleBackward() { doBackward(); server.send(200,"text/plain","B"); }
void handleLeft()     { doLeft();     server.send(200,"text/plain","L"); }
void handleRight()    { doRight();    server.send(200,"text/plain","R"); }
void handleStop()     { doStop();     server.send(200,"text/plain","S"); }

// ======================================================
// MQTT
// ======================================================
void reconnect() {
  int attempts = 0;
  while (!client.connected() && attempts < 3) {
    Serial.print("TB connecting...");
    if (client.connect("ESP32BlackBox", token, NULL)) {
      Serial.println("✅");
    } else {
      Serial.printf("❌ rc=%d\n", client.state());
      attempts++;
      delay(2000);
    }
  }
}

void sendDataTB() {
  char payload[300];
  snprintf(payload, sizeof(payload),
    "{\"temp\":%.2f,\"vib\":%d,\"gforce\":%.3f,\"status\":\"%s\"}",
    latestTemp, latestVib, gForce, systemStatus.c_str()
  );
  Serial.print("TB: ");
  Serial.println(payload);
  if (!client.publish("v1/devices/me/telemetry", payload)) {
    Serial.println("❌ TB send FAILED");
    client.disconnect();
  }
}

// ======================================================
// WEB API + UI
// ======================================================
void sendWebData() {
  char json[200];
  snprintf(json, sizeof(json),
    "{\"time\":\"%s\",\"temp\":%.2f,\"vib\":%d,"
    "\"gforce\":%.3f,\"status\":\"%s\"}",
    latestTime.c_str(), latestTemp, latestVib,
    gForce, systemStatus.c_str()
  );
  server.send(200, "application/json", json);
}

void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Black Box Dashboard</title>
<style>
body{background:#0f172a;font-family:Arial;text-align:center;color:white;margin:0;}
.card{background:#1e293b;padding:15px;margin:10px auto;width:260px;border-radius:15px;font-size:18px;}
button{width:110px;height:65px;margin:10px;font-size:16px;border-radius:12px;border:none;background:#374151;color:white;}
.stop{background:#dc2626;}
button:active{transform:scale(0.95);background:#2563eb;}
</style>
<script>
function send(cmd){fetch("/"+cmd);}
function updateData(){
  fetch("/data")
  .then(r=>r.json())
  .then(data=>{
    document.getElementById("time").innerText=data.time;
    document.getElementById("temp").innerText=data.temp+" C";
    document.getElementById("vib").innerText=data.vib;
    document.getElementById("acc").innerText=data.gforce+" G";
    document.getElementById("status").innerText=data.status;
  });
}
setInterval(updateData,1000);
</script>
</head>
<body>
<h2>Black Box Dashboard</h2>
<div class="card">Time: <span id="time">--</span></div>
<div class="card">Temperature: <span id="temp">--</span></div>
<div class="card">Vibration: <span id="vib">--</span></div>
<div class="card">Acceleration: <span id="acc">--</span></div>
<div class="card">Status: <span id="status">--</span></div>
<br>
<div>
<button onclick="send('F')">Forward</button><br>
<button onclick="send('L')">Left</button>
<button class="stop" onclick="send('S')">Stop</button>
<button onclick="send('R')">Right</button><br>
<button onclick="send('B')">Backward</button>
</div>
</body>
</html>
)rawliteral";
  server.send(200, "text/html", html);
}

// ======================================================
// SETUP
// ======================================================
void setup() {
  Serial.begin(115200);

  // Motor pins first
  pinMode(in1, OUTPUT); pinMode(in2, OUTPUT);
  pinMode(in3, OUTPUT); pinMode(in4, OUTPUT);
  doStop();  // ensure motors off at boot

  pinMode(VIB_PIN,   INPUT);
  pinMode(ALERT_PIN, OUTPUT);
  digitalWrite(ALERT_PIN, LOW);

  mpu_init();

  client.setBufferSize(512);

  WiFi.begin(ssid, password);
  Serial.print("WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\n✅ WiFi: " + WiFi.localIP().toString());

  configTime(19800, 0, "pool.ntp.org");
  delay(2000);

  // SD init AFTER WiFi — SPI bus is more stable
  sd_init();

  client.setServer(mqtt_server, 1883);

  server.on("/",    handleRoot);
  server.on("/data",handleWebData);
  server.on("/F",   handleForward);
  server.on("/B",   handleBackward);
  server.on("/L",   handleLeft);
  server.on("/R",   handleRight);
  server.on("/S",   handleStop);
  server.begin();
  Serial.println("✅ Web server started");
}

// ======================================================
// LOOP
// ======================================================
void loop() {

  server.handleClient();  // always first, keep responsive

  if (!client.connected()) reconnect();
  client.loop();

  // -------- SENSORS --------
  int adc = analogRead(TEMP_PIN);
  float voltage     = adc * (3.3 / 4095.0);
  float temperature = (voltage - 0.5) * 100;

  int vibration = digitalRead(VIB_PIN);
  if (vibration == HIGH) {
    vibrationCount++;
  } else {
    static unsigned long lastDecay = 0;
    if (millis() - lastDecay >= 5000) {
      if (vibrationCount > 0) vibrationCount--;
      lastDecay = millis();
    }
  }
  if (vibrationCount > peakVib) peakVib = vibrationCount;

  mpu_read();

  latestTemp = temperature;
  latestVib  = vibrationCount;
  latestTime = getTime();

  // -------- RING BUFFER --------
  ring_push(latestTime, latestTemp, latestVib,
            gForce, systemStatus);

  // -------- STATUS --------
  if (!crashDetected) {
    bool vibCrash = (vibrationCount >= 7);
    bool mpuCrash = (gForce > 2.5);

    if (vibCrash || mpuCrash) {
      crashDetected = true;
      systemStatus  = "CRASH DETECTED";
      Serial.println("🚨 CRASH DETECTED");
      if (mpuCrash) Serial.printf("   G: %.3f\n", gForce);
      if (vibCrash) Serial.printf("   Vib: %d\n", vibrationCount);
      digitalWrite(ALERT_PIN, HIGH);
      doStop();  // stop motors on crash
      crashStartTime = millis();
      restartPending = true;
      ring_dump_to_sd();  // dump pre-crash history
    }
    else if (temperature > 38) {
      systemStatus = "HIGH TEMP";
    }
    else {
      systemStatus = "NORMAL";
    }
  }

  // -------- SD WRITE every second (always) --------
  if (millis() - lastSDWrite >= SD_WRITE_INTERVAL) {
    lastSDWrite = millis();
    sd_writeLine(latestTime.c_str(), latestTemp,
                 latestVib, gForce, systemStatus.c_str());
  }

  // -------- TB SEND every 3 seconds --------
  if (millis() - lastSendTime >= SEND_INTERVAL) {
    lastSendTime = millis();
    latestVib = peakVib;
    sendDataTB();
    peakVib   = 0;
    latestVib = vibrationCount;
  }

  // -------- AUTO RESTART --------
  if (restartPending && millis() - crashStartTime >= 120000) {
    Serial.println("🔁 Restarting...");
    digitalWrite(ALERT_PIN, LOW);
    delay(500);
    ESP.restart();
  }

  // -------- SERIAL --------
  Serial.printf("%s | T:%.2f | V:%d | G:%.3f | %s\n",
    latestTime.c_str(), latestTemp,
    vibrationCount, gForce, systemStatus.c_str());

  delay(1000);  // reduced from 1000 — keeps web server responsive
}

// ---- missing forward declaration fix ----
void handleWebData() { sendWebData(); }