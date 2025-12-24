EggLink - IoT Climate Monitoring System

This repository centralizes the source code developed for the Environmental Monitoring and Fire Prevention System, conceived within the scope of the Projeto Integrativo IV (Integrative Project IV) course, part of the Percurso Competências (Competencies Track - Pilot) at the Escola Politécnica da USP (Polytechnic School of the University of São Paulo).

The project consists of implementing a low-power mesh network based on the OpenThread protocol, utilizing ESP32-C6 microcontrollers for real-time monitoring of microclimatic variables, such as temperature, air humidity, soil moisture, and air quality.

Development Team
The system was architected and developed by the students of the Percurso Competências:

Isadora Ribeiro Vital

Janos Biezok Neto

Jefferson Santos Monteiro

Jorge Ricardo Barbosa França

Repository Structure
For organization, maintenance, and evaluation purposes, the project was structured as a monorepo, divided into three main directories corresponding to the functional modules of the system:

1. ot_cli_gateway_final (Border Router)
This directory contains the firmware developed for the central node (Gateway). The code is responsible for managing the coexistence of Wi-Fi and IEEE 802.15.4 radios, acting as a bridge between the local mesh network and the internet. Its main functions include receiving packets via the CoAP protocol and forwarding processed data to the external server via HTTP POST requests.

2. ot_cli_nos_final (Sensor Nodes)
This directory stores the replicable firmware intended for the distributed sensor nodes. The software manages peripheral readings—including the MQ-135 analog gas sensor, the TDS sensor adapted for soil moisture, and the digital AHT25 sensor via I2C—and executes power-saving routines (Deep Sleep) and data transmission via the Thread network.

3. egglink_server (Backend & Dashboard)
This directory contains the complete web server application. The backend was developed in Python using the Flask framework, responsible for receiving data from the Gateway, executing fire alert detection logic, and integrating with the Google Sheets API for historical persistence. The directory also includes frontend files for the Dashboard for device visualization and geolocation.

Technologies and Tools
Hardware: Espressif ESP32-C6 (RISC-V)

Firmware: ESP-IDF v5.x

Protocols: OpenThread (IEEE 802.15.4), CoAP, UDP, Wi-Fi, HTTP

Software: C/C++, Python (Flask), Google Apps Script

Credits and Third-Party References
The development of this project relied on open-source community libraries and examples. Specifically, the implementation of the I2C communication driver for the temperature and humidity sensors (AHT25) was adapted from the espidf_beginer repository, authored by pangcrd (https://github.com/pangcrd/espidf_beginer). The original code was modified to suit the Real-Time Operating System (FreeRTOS) used in this project.

Note on Versioning
This repository consolidates modules that were developed in separate environments during the prototyping phase. Temporary build files, binaries, and local dependencies were excluded via .gitignore to ensure source code integrity and cleanliness.


