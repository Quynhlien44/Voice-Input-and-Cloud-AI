from speech_to_text import record_and_recognize
from intent_detector import detect_intent
from action_executor import execute_action
from ai_backend import ai_backend_response
from serial_comm import SerialComm
import time

def main_loop(serial_port="/dev/ttyACM0"):
    ser = SerialComm(serial_port)

    try:
        while True:
            command_from_esp = ser.receive()
            if command_from_esp:
                print("Received command from ESP32:", command_from_esp)

                if command_from_esp == "VOICE_COMMAND":
                    # Voice recording and recognition
                    text = record_and_recognize()
                else:
                    # Direct text command from ESP32
                    text = command_from_esp

                print("Recognized text/input:", text)

                # Define intent
                intent, action = detect_intent(text)
                print("Detected intent:", intent, "action:", action)

                if intent:
                    # Execute local command
                    response = execute_action(action)
                else:
                    # Call AI backend if intent is not recognized
                    response = ai_backend_response(text)

                print("Response to send:", response)

                # Send response to ESP32
                ser.send(response)

            time.sleep(0.1)

    except Exception as e:
        print("Error in main_loop:", e)

    finally:
        ser.close()
        print("Program stopped.")

if __name__ == "__main__":
    main_loop(serial_port="/dev/ttyACM0")
