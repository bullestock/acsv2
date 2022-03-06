import serial
import threading
import time

class RfidReader(threading.Thread):
    
    def __init__(self, serial_port = "/dev/ttyUSB0"):
        threading.Thread.__init__(self)
        self.daemon = True
        self.lock = threading.Lock()
        self.tag_id = ''
        self.last = time.time()
        print("Opening serial port")
        self.ser = serial.Serial(port = serial_port,
            baudrate = 115200,
            timeout = 1.0)
        self.ser.flushInput()
        print("Opened serial port")
        self.ser.setDTR(False)
        # Reset
        self.ser.setRTS(True)
        time.sleep(0.1)
        self.ser.setRTS(False)
        # Skip ESP prologue
        while True:
            time.sleep(0.1)
            line = self.ser.readline()
            #print(f"Got {line}")
            if len(line) == 0:
                break
        print("Skipped prologue")
        # Get version
        while True:
            time.sleep(0.1)
            print("Send V")
            self.ser.write(b"V\n")
            time.sleep(0.5)
            reply = self.ser.readline()
            #print(f"Got #{line} -> #{reply}")
            if reply == "V":
                # Echo is on
                line = self.ser.readline()
                reply = ''.join(filter(lambda x: x in printable, reply))
            if b"ACS" in reply:
                print(f"Version: #{reply}")
                if b"cardreader" in reply:
                    break
        self.ser.reset_input_buffer()
            
    def getid(self):
        id = ''
        self.lock.acquire()
        # Code expires after 10 seconds
        if time.time() - self.last < 10:
            id = self.tag_id
            self.tag_id = ''
        self.lock.release()
        return id
    
    def read_id(self):
        self.ser.write(b"C\n")
        line = self.ser.read_until().decode("utf-8") 
        line = line.strip()
        if line[0:2] == 'ID':
            line = line[2:]
        return line

    def get_version(self):
        self.ser.write(b"V\n")
        line = self.ser.read_until().decode("utf-8") 
        line = line.strip()
        return line

    def run(self):
        while True:
            i = self.read_id()
            if len(i) > 0:
                self.lock.acquire()
                self.tag_id = i
                self.last = time.time()
                self.lock.release()
            time.sleep(0.1)
            
if __name__ == "__main__":
    l = RfidReader()
    l.start()
    
    print("Version %s" % l.get_version())
    while True:
        print("ID %s" % l.getid())
        time.sleep(2)
