import time
from datetime import datetime
from rfidreader import RfidReader
from slack import Slack
from rest import RestClient
from display import Display
import io
import sys
import urllib3

is_raspberrypi = False
try:
    with io.open('/sys/firmware/devicetree/base/model', 'r') as m:
        if 'raspberry pi' in m.read().lower():
            is_raspberrypi = True
except Exception: pass

is_orangepi = False
try:
    with io.open('/sys/firmware/devicetree/base/model', 'r') as m:
        if 'orange pi' in m.read().lower():
            is_orangepi = True
except Exception: pass

if is_raspberrypi:
    from gpiozero import LED
    from display import Display
    lock = LED(4)

if is_orangepi:
    import OPi.GPIO as GPIO
    from display import Display
    GPIO.setmode(GPIO.SUNXI)
    GPIO.setup('PA00', GPIO.OUT)
    GPIO.setup('PA01', GPIO.OUT)

TIMEOUT = 10
LOG_TIMEOUT = 5*60

VERSION = "0.9"

def set_lock(on):
    if is_raspberrypi:
        if on:
            lock.on()
        else:
            lock.off()
    else:
        GPIO.output('PA00', on)

reader = RfidReader()
reader.start()

restclient = RestClient()

disp = Display()
disp.println("BACS %s ready" % VERSION)

slack = Slack()

last_card_id = None
last_card_time = time.time() - TIMEOUT
last_log_time = time.time() - LOG_TIMEOUT
print("%s Started" % datetime.now())
sys.stdout.flush()
slack.set_status(":farmer: BACS version %s starting" % VERSION)
while True:
    time.sleep(0.1)
    card_id = reader.getid()
    if len(card_id) > 0:
        if time.time() - last_card_time > TIMEOUT:
            last_card_id = None
        if card_id != last_card_id:
            print("%s Card ID %s" % (datetime.now(), card_id))
            sys.stdout.flush()
            disp.println("Checking card...")
            last_card_id = card_id
            last_card_time = time.time()
            try:
                r = restclient.check_card(card_id)
            except:
                disp.println("Error accessing ACS")
                time.sleep(1)
                continue
            msg = ''
            if r['id'] == 0:
                print("Card not found")
                sys.stdout.flush()
                disp.println("Card %s not found" % card_id)
                slack.set_status(":tractor: BACS: Unrecognized card %s" % card_id)
                msg = "BACS: Unrecognized card %s" % card_id
            else:
                print("Card found")
                sys.stdout.flush()
                username = r['name']
                disp.println("User: %s" % username)
                if r['allowed']:
                    user_approved = True
                    msg = "BACS: Granted entry"
                    slack.set_status(":door: BACS: Valid card swiped, unlocking")
                else:
                    user_approved = False
                    disp.println("Not allowed")
                    msg = "BACS: Entry not allowed"
                    slack.set_status(":no_entry: BACS: Invalid card swiped")
                if user_approved:
                    disp.println("Opening")
                    set_lock(True)
                    time.sleep(10)
                    disp.println("Closing")
                    set_lock(False)
                if time.time() - last_log_time > LOG_TIMEOUT:
                    try:
                        id = None
                        if 'id' in r:
                            id = r['id']
                        r = restclient.log(id, msg)
                    except:
                        disp.println("Error accessing ACS")
                        time.sleep(1)

