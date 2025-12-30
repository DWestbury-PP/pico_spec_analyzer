#!/usr/bin/env python3
"""
Simple serial reader for debugging Pico output
"""
import serial
import sys
import glob

def find_pico_port():
    """Find the Pico's serial port"""
    ports = glob.glob('/dev/tty.usbmodem*')
    if ports:
        return ports[0]
    ports = glob.glob('/dev/ttyACM*')
    if ports:
        return ports[0]
    return None

def main():
    port = find_pico_port()
    if not port:
        print("Error: Pico serial port not found")
        sys.exit(1)
    
    print(f"Reading from {port} @ 115200 baud")
    print("Press Ctrl+C to exit\n")
    print("-" * 50)
    
    try:
        with serial.Serial(port, 115200, timeout=1) as ser:
            while True:
                if ser.in_waiting:
                    line = ser.readline().decode('utf-8', errors='ignore').rstrip()
                    print(line)
    except KeyboardInterrupt:
        print("\n\nExiting...")
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    main()

