from speech_to_text import record_and_recognize
from intent_detector import detect_intent
from action_executor import execute_action
from ai_backend import ai_backend_response
from serial_comm import SerialComm
import time

def main_loop(serial_port="/dev/ttyACM0"):  # Adjust port according to OS
    ser = SerialComm(serial_port)

    try:
        while True:
            command_from_esp = ser.receive()
            if command_from_esp:
                print("Received command from ESP32:", command_from_esp)
                if command_from_esp == "VOICE_COMMAND":
                    text = record_and_recognize()
                    print("Recognized text:", text)
                    intent, action = detect_intent(text)
                    print("Detected intent:", intent, "action:", action)
                    if intent:
                        response = execute_action(action)
                    else:
                        response = ai_backend_response(text)
                    print("Response to send:", response)
                    ser.send(response)
                else:
                    response = ai_backend_response(command_from_esp)
                    print("Response to send:", response)
                    ser.send(response)
            time.sleep(0.1)

    except Exception as e:
        print("Error in main_loop:", e)
    finally:
        ser.close()
        print("Program stopped.")

if __name__ == "__main__":
    main_loop(serial_port="/dev/ttyACM0")
