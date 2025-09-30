# Voice Input and Cloud AI Project

---

## Overview

This project builds an ESP32-S3 voice control hardware and a PC software acquisition system in Python, using cloud AI like ChatGPT or Grok for natural language processing.

---

## Structure

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

## Setup Instructions

1. ESP32 Firmware
- Build and flash the firmware in main/ folder using ESP-IDF tools.
- The firmware reads voice/text commands and sends to PC via serial port.
- When INMP441 microphone hardware becomes available, firmware will update to support audio streaming on ESP32.
2. Python PC Software
- Create and active a Python virtual environment:

```bash
python3 -m venv venv
source venv/bin/activate
```

- Install required packages:

```bash
pip install -r python_pc_bridge/requirements.txt
```

- Run the PC software:

```bash
python3 python_pc_bridge/main.py
```

---

## How It Works

- ESP32 sends voice or text commands to PC via serial.
- PC records voice using Whisper or processes text commands.
- Intent detector parses command meaning
- Executes local device actions or calls cloud AI services.
- Cloud AI (ChatGPT/Grok) provides natural language response.
- Response sent back to ESP32 for LED control or TTS playback.

---

## Environment Variables

To use AI cloud services, you need to setup API keys securely:
Create a .env.local file in the root directory and add keys there:

```bash
OPENAI_API_KEY='your-openai-api-key-here'
GROK_API_COOKIE='your-grok-cookie-here'
```

Note: Make sure to never share your API keys publicly.

---

## Note

- When the INMP441 microphone device is ready, the ESP32 firmware will update to record directly to the PC.
- Currently, the PC uses the Whisper model to record from the built-in microphone/USB.
- The modules are easy to expand and update when needed.