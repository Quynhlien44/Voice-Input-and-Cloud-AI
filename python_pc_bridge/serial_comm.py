import serial
import time

class SerialComm:
    def __init__(self, port, baudrate=115200, timeout=1):
        self.ser = serial.Serial(port, baudrate, timeout=timeout)

    def send(self, msg):
        if not msg.endswith("\n"):
            msg += "\n"
        self.ser.write(msg.encode())

    def receive(self):
        if self.ser.in_waiting > 0:
            return self.ser.readline().decode().strip()
        return None

    def close(self):
        self.ser.close()

if __name__ == "__main__":
    # Test serial communication only
    ser = SerialComm('/dev/ttyACM0')  
    try:
        while True:
            received = ser.receive()
            if received:
                print(f"Received from ESP32: {received}")
                if received == "Turn on light":
                    ser.send("Light is ON")
                else:
                    ser.send("Unknown command")
            time.sleep(0.1)
    except KeyboardInterrupt:
        ser.close()
