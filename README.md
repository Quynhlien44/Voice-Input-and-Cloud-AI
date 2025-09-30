# Voice Input and Cloud AI Project

---

## Overview

This project builds an ESP32-S3 voice control hardware and a PC software acquisition system in Python, using cloud AI like ChatGPT or Grok for natural language processing.

---

## Structure

`text.
├── main/                     # Firmware ESP32 (written in C using ESP-IDF)
│   ├── main.c                # Code main firmware ESP32
│   ├── CMakeLists.txt        # ESP-IDF build configuration
├── python_pc_bridge/         # Python software running on PC
│   ├── main.py               # Process coordinated commands, receive ESP32 data
│   ├── ai_backend.py         # Call API ChatGPT/Grok
│   ├── speech_to_text.py     # Voice recording and recognition using OpenAI Whisper
│   ├── intent_detector.py    # Detect command intent from text
│   ├── action_executor.py    # Fast local command execution
│   ├── serial_comm.py        # Communicate with ESP32 via serial
│   └── requirements.txt      # Library to install for PC
├── README.md                 # General information about the project`

---

## Sample Project

- This sample project is located in the `main/` directory with the `main.c` file.
- This is a starter sample project for developing with ESP-IDF for ESP32-S3.
- Use CMake to build, configure in the `CMakeLists.txt` file.
- Details on how to build and create a new project according to the ESP-IDF documentation:

https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html#start-a-new-project

---

## Instructions

1. Install the ESP-IDF environment for ESP32-S3 according to the manual.
2. Build and flash the ESP32 firmware in the `main` folder.
3. On your PC, create a Python virtual environment and install the library:

`pip install -r python_pc_bridge/requirements.txt`

1. Run Python software on PC:

`python3 python_pc_bridge/main.py`

1. ESP32 sends voice or text commands via serial port to PC for processing.
2. PC records voice or text in real time, calls AI for translation, receives intent analysis, and then returns commands to ESP32.

---

## Note

- When the INMP441 microphone device is ready, the ESP32 firmware will update to record directly to the PC.
- Currently, the PC uses the Whisper model to record from the built-in microphone/USB.
- The modules are easy to expand and update when needed.