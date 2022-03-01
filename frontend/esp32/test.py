import serial
import threading
import time

print("Opening serial port")
ser = serial.Serial(port = "/dev/ttyUSB0",
                    baudrate = 115200,
                    timeout = 1.0)
ser.flushInput()
print("Opened serial port")
ser.setDTR(False)
# Reset
ser.setRTS(True)
time.sleep(0.1)
ser.setRTS(False)
# Skip ESP prologue
while True:
    time.sleep(0.01)
    line = ser.readline()
    #print(f"Got {line}")
    if len(line) == 0:
        break
print("Skipped prologue")
# Get version
while True:
    time.sleep(0.1)
    print("Send V")
    ser.write(b"V\n")
    time.sleep(0.5)
    reply = ser.readline()
    #print(f"Got #{line} -> #{reply}")
    if reply == "V":
        # Echo is on
        line = ser.readline()
        reply = ''.join(filter(lambda x: x in printable, reply))
    if b"ACS" in reply:
        print(f"Version: #{reply}")
        if b"display" in reply:
            break
ser.reset_input_buffer()

ser.write(b"C\n")

# Large text
if False:
    for i in range(0, 8):
        ser.write(b"T0,%d,%d,X,;:''XXXXXXXX%d\n" % (i, i % 8, i))
    for i in range(0, 8):
        time.sleep(1)
        ser.write(b"TE0,%d,%d,HHHHHHHHH\n" % (i, i % 8))

# Small text
if True:
    for i in range(0, 14):
        ser.write(b"t0,%d,%d,X,;:''XXXXXXXX%d\n" % (i, i % 8, i))
    for i in range(0, 14):
        time.sleep(1)
        ser.write(b"te0,%d,%d,HHHHHHHHH\n" % (i, i % 8))
    

# y = 7
# for i in range(0, 9):
#     ser.write(b"r0,%d,200,%d,%d\n" % (y, y+24, i % 8))
#     y = y + 25

