# 🚗 Automotive Black Box (EDR) with Wireless Vehicle Control System

## 📖 Overview

This project implements a low-cost **Automotive Event Data Recorder (EDR)** using the ESP32 microcontroller. The system continuously monitors vehicle conditions through multiple sensors and records critical information before, during, and after crash events.

The project combines **Embedded Systems**, **IoT**, **Real-Time Monitoring**, **Data Logging**, and **Wireless Vehicle Control** into a single platform.

When a crash is detected, the system:

* Stops vehicle movement automatically
* Activates an alert indication
* Stores crash information on an SD card
* Saves pre-crash history using a ring buffer
* Uploads telemetry to ThingsBoard Cloud
* Displays live data through a local web dashboard

---

## ✨ Features

* ESP32-based architecture
* Dual-condition crash detection
* Real-time sensor monitoring
* Pre-crash data storage using Ring Buffer
* SD Card data logging
* MQTT cloud communication
* ThingsBoard integration
* Local Web Dashboard
* Wireless vehicle control
* Automatic safety response
* Automatic system restart after crash event

---

## 🛠 Hardware Components

| Component          | Purpose                          |
| ------------------ | -------------------------------- |
| ESP32              | Main Controller                  |
| MPU6050            | Acceleration & G-Force Detection |
| Vibration Sensor   | Impact Detection                 |
| Temperature Sensor | Thermal Monitoring               |
| SD Card Module     | Event Data Storage               |
| L298N Motor Driver | Vehicle Control                  |
| DC Motors          | Vehicle Simulation               |
| Alert LED          | Crash Indication                 |

---

## 🔌 Pin Connections

| Component          | ESP32 Pin |
| ------------------ | --------- |
| Temperature Sensor | GPIO 34   |
| Vibration Sensor   | GPIO 27   |
| Alert LED          | GPIO 14   |
| Motor Driver IN1   | GPIO 26   |
| Motor Driver IN2   | GPIO 25   |
| Motor Driver IN3   | GPIO 33   |
| Motor Driver IN4   | GPIO 32   |
| MPU6050 SDA        | GPIO 21   |
| MPU6050 SCL        | GPIO 22   |
| SD Card CS         | GPIO 5    |

### MPU6050 (I2C Interface)

| Signal | ESP32 Pin |
| ------ | --------- |
| SDA    | GPIO 21   |
| SCL    | GPIO 22   |

### SD Card Module (SPI Interface)

| Signal | ESP32 Pin      |
| ------ | -------------- |
| CS     | GPIO 5         |
| MOSI   | ESP32 SPI MOSI |
| MISO   | ESP32 SPI MISO |
| SCK    | ESP32 SPI SCK  |

---

## 🏗 System Architecture

The ESP32 acts as the central controller and interfaces with:

* Temperature Sensor
* Vibration Sensor
* MPU6050 Accelerometer
* SD Card Module
* ThingsBoard Cloud
* Local Web Dashboard
* L298N Motor Driver
* Alert System

### Architecture Diagram

![System Architecture](images/system_architecture.png)

---

## ⚙ Working Principle

### Step 1: Data Acquisition

The ESP32 continuously acquires data from:

* Temperature Sensor
* Vibration Sensor
* MPU6050 Accelerometer

### Step 2: Ring Buffer Storage

A circular buffer stores approximately **3 minutes of recent sensor data** in memory.

### Step 3: Crash Detection

A crash is detected when:

* Vibration exceeds the predefined threshold

OR

* G-Force exceeds the predefined threshold

### Step 4: Crash Response

Upon crash detection:

* Motors are stopped
* Alert LED is activated
* Pre-crash data is preserved
* Event data is logged to SD card
* Cloud dashboard is updated

### Step 5: Monitoring

The system supports:

* Local Web Dashboard (HTTP)
* ThingsBoard Cloud Dashboard (MQTT)

### Step 6: Recovery

After a predefined delay, the system automatically restarts and resumes monitoring.

---

## 📊 Technologies Used

### Embedded Systems

* ESP32
* Embedded C/C++

### Communication

* Wi-Fi
* MQTT
* HTTP

### Protocols

* I2C
* SPI

### Cloud Platform

* ThingsBoard

### Web Technologies

* HTML
* CSS
* JavaScript

### Time Synchronization

* NTP (Network Time Protocol)

---

## 📸 Project Images

### Hardware Setup

![Hardware Setup](images/hardware_setup.jpg)

### Local Dashboard

![Dashboard](images/local_dashboard.png)

### ThingsBoard Dashboard

![ThingsBoard](images/thingsboard_dashboard.png)

### Circuit Diagram

![Circuit Diagram](images/circuit_diagram.png)

---

## 📈 Key Features Implemented

✅ Dual-Sensor Crash Detection

✅ Pre-Crash Data Storage

✅ SD Card Event Logging

✅ MQTT Cloud Monitoring

✅ Real-Time Dashboard

✅ Wireless Vehicle Control

✅ Automatic Safety Response

✅ Auto Restart Mechanism

---

## 📄 Documentation

Detailed project report:

[📘 Project Report](docs.pdf)

---

## 🚀 Future Improvements

* GPS Integration
* GSM Emergency Alert System
* Camera-Based Recording
* CAN Bus Integration
* FreeRTOS Task Scheduling
* Mobile Application
* AI-Based Crash Prediction
* Industrial Grade Sensors
* Secure Cloud Data Storage

---

## 🎯 Applications

* Vehicle Event Data Recording
* Accident Analysis
* Automotive Research
* Driver Safety Systems
* Embedded Systems Education
* IoT-Based Vehicle Monitoring

---

## 👨‍💻 Author

**A. Lakshmi Sai Ganesh**

Electronics & Communication Engineering

ESP32 | Embedded Systems | IoT | Automotive Electronics | Firmware Development

---

## ⭐ Project Goal

To develop a low-cost educational prototype of an Automotive Event Data Recorder (EDR) capable of crash detection, event logging, cloud monitoring, and wireless vehicle control using modern Embedded and IoT technologies.
