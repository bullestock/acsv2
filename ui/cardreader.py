import serial, sys, time
import requests

class CardReader:
    def __init__(self, serial_port):
        # Open port with default baud rate
        self.port = serial.Serial(serial_port)
        # Reset Arduino
        self.port.setDTR(False)
        time.sleep(1)
        self.port.flushInput()
        self.port.setDTR(True)
        # Reopen with proper baud rate
        self.port = serial.Serial(port = serial_port,
            baudrate = 115200,
            timeout = 1.0,
            rtscts = 1,
            dsrdtr = False)
            
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
        self.port.write(b"C\n")
        line = self.port.read_until().decode("utf-8") 
        line = line.strip()
        if line[0:2] == 'ID':
            line = line[2:]
        return line


    def set_ui(ui):
        self.ui = ui

    def send(s):
        self.port.flushInput()
        #print(f"Send({Time.now}): {s}"
        self.port.write(s + "\n")
        self.port.flush()
        while True:
            line = self.port.readline()
            if line and len(line) > 0:
                break
        line = line.strip()
        #print(f"Reply: {line}")
        if line != "OK":
            print(f"ERROR: Expected 'OK', got '{line}' (in response to {s})")
            sys.exit()
  
    def warn_closing():
        set_led(LED_CLOSING)
  
    def advertise_open():
        set_led(LED_OPEN)

    def advertise_ready():
        set_led(LED_READY)

    def check_permission(id, api_key):
        rest_start = time.now()
        allowed = False
        who = ''
        try:
            session = requests.Session()
            session.verify = False
            response = requests.post(f"{HOST}/api/v1/permissions",
                                     data = { 'api_token': api_key,
                                              'card_id': id
                                             },
                                     headers = {
                                         'Content-Type': 'application/json',
                                         'Accept': 'application/json'
                                     },
                                     timeout = 60)
            print(f"Got server reply in {time.now() - rest_start} s")
        except Exception as e:
            print(f"Failed to connect to server: {e}")
            return False
        print(response)
        #!!
        return false #allowed, error, who, user_id
  
    def add_unknown_card(api_key, id):
        rest_start = time.now()
        error = False
        try:
            session = requests.Session()
            session.verify = False
            response = requests.post(f"{HOST}/api/v1/unknown_cards",
                                     data = { 'api_token': api_key,
                                              'card_id': id
                                             },
                                     headers = {
                                         'Content-Type': 'application/json',
                                         'Accept': 'application/json'
                                     },
				     timeout = 60)
            print(f"Got server reply in {time.now() - rest_start} s")
        except Exception as e:
            print(f"Failed to connect to server: {e}")
            error = true
        return not error

    def set_led(cmd):
        if cmd == self.last_led_cmd:
            return
        self.last_led_cmd = cmd
        self.port.write(cmd + "\n")

    def update(q, api_key):
        if time.now() - self.last_card_read_at < 1:
            return
        self.last_card_read_at = Time.now
        self.port.flushInput()
        self.port.write("C\n")
        #print("Send({time.now()}): C")
        while True:
            line = self.port.readline()
            if line and len(line) > 0:
                break
        line = line.strip()
        #print(f"Reply: {line}")
        if not line or len(line) == 0 or line[0:1] != "ID":
            print(f"ERROR: Expected 'IDxxxxxx', got '{line}' (in response to C)")
            return
        line = line[2:-1]
        if len(line) > 0 and len(line) != 10:
            print(f"Invalid card ID: {line}")
            line = ''
        now = time.now()
        if len(line) > 0 and ((line != self.last_card) or (now - self.last_card_seen_at > 5)):
            print(f"Card ID: {line}")
            self.last_card = line
            self.last_card_seen_at = now
            # Let user know we are doing something
            set_led(LED_WAIT)
            allowed, error, who, user_id = check_permission(self.last_card, api_key)
            if error:
                set_led(LED_ERROR)
            else:
                if allowed == true:
                    set_led(LED_ENTER)
                    self.ui.unlock(who)
                    #add_log(q, user_id, 'Granted entry')
                elif allowed == false:
                    set_led(LED_NO_ENTRY)
                    if user_id:
                        #add_log(q, user_id, 'Denied entry')
                        self.ui.set_temp_status(['Denied entry:', who], 'red')
                    else:
                        #add_log(q, user_id, "Denied entry for {self.last_card}")
                        add_unknown_card(api_key, self.last_card)
                        self.ui.set_temp_status(['Unknown card', self.last_card], 'yellow')
                else:
                    print(f"Impossible! allowed is neither true nor false: {allowed}")
                    set_led(LED_ERROR)
