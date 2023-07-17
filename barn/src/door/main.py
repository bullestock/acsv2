import io
import sys
import time
import urllib3
from datetime import datetime

import OPi.GPIO as GPIO
GPIO.setmode(GPIO.SUNXI)
GPIO.setup('PA00', GPIO.OUT)
GPIO.setup('PA01', GPIO.OUT)
GPIO.setup('PA03', GPIO.IN)
GPIO.setup('PA06', GPIO.IN)

sys.path.append('..')
from display import Display
from gateway import Gateway
from rest import RestClient
from rfidreader import RfidReader
from slack import Slack

# How many seconds must pass before a new swipe is accepted?
TIMEOUT = 5
# How often do we log to ACS backend?
LOG_TIMEOUT = 1*60
# How often do we contact the gateway?
PING_INTERVAL = 1*60
# How many seconds can the door stay open before we complain?
MAX_OPEN_TIME = 5*60

VERSION = "0.5"

def set_lock(on):
    GPIO.output('PA01', on)

def is_button_pressed():
    return GPIO.input('PA06')

def is_door_closed():
    return not GPIO.input('PA03')

disp = Display()

gw = Gateway('barndoor')
slack = Slack(gw)

try:
    reader = RfidReader()
    reader.start()
except:
    disp.println("RFID reader error")
    slack.send_message(":stop: BarnDoor: RFID reader not found")
    time.sleep(10)
    
restclient = RestClient()

disp.println("BarnDoor %s ready" % VERSION)

last_card_id = None
last_card_time = time.time() - TIMEOUT
last_log_time = time.time() - LOG_TIMEOUT
last_gw_ping = time.time() - PING_INTERVAL
last_closed_time = None
last_open_warning = None

gw.log("%s Started" % datetime.now().strftime("%Y-%m-%d %H:%M:%S"))
sys.stdout.flush()
slack.send_message(":ladeport: BarnDoor version %s starting" % VERSION)
# How often do we complain about an open door?
open_warning_interval = 5*60
while True:
    time.sleep(0.1)

    # Check if door has been open for too long
    if is_door_closed():
        if last_open_warning:
            slack.send_message(':ladeport: Barn door is closed')
            gw.log('Door is closed')
            open_warning_interval = 5*60
        last_closed_time = time.time()
        last_open_warning = None
    else:
        if last_closed_time:
            open_for = time.time() - last_closed_time
            if open_for > MAX_OPEN_TIME:
                if not last_open_warning or (time.time() - last_open_warning >= open_warning_interval):
                    slack.send_message(':ladeport: Barn door has been open for %d minutes' %
                                       int(open_for/60))
                    gw.log('Door has been open for %d minutes' % int(open_for/60))
                    last_open_warning = time.time()
                    open_warning_interval *= 2
        
    card_id = reader.getid()
    if len(card_id) > 0:
        if time.time() - last_card_time > TIMEOUT:
            last_card_id = None
        if card_id != last_card_id:
            print("%s Card ID %s" % (datetime.now().strftime("%Y-%m-%d %H:%M:%S"), card_id))
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
                log_msg = "Card %s not found" % card_id
                disp.println(log_msg)
                gw.log(log_msg)
                slack.set_status(":ladeport: BarnDoor: Unrecognized card %s" % card_id)
                msg = "BACS: Unrecognized card %s" % card_id
            else:
                print("Card found")
                sys.stdout.flush()
                username = r['name']
                disp.println("User: %s" % username)
                if r['allowed']:
                    user_approved = True
                    msg = "BarnDoor: Granted entry"
                else:
                    user_approved = False
                    disp.println("Not allowed")
                    msg = "BarnDoor: Entry not allowed"
                    slack.set_status(":no_entry: BarnDoor: Invalid card swiped")
                    gw.log('Card %s not allowed' % card_id)
                if user_approved:
                    disp.println("Opening")
                    set_lock(True)
                    slack.set_status(":ladeport: BarnDoor: Valid card swiped, unlocking")
                    gw.log('Unlocked')
                    time.sleep(10)
                    disp.println("Closing")
                    set_lock(False)
                    gw.log('Locked')
                if time.time() - last_log_time > LOG_TIMEOUT:
                    last_log_time = time.time()
                    slack.set_status(None)
                    try:
                        id = None
                        if 'id' in r:
                            id = r['id']
                        r = restclient.log(id, msg)
                    except e as Exception:
                        disp.println("Error accessing ACS")
                        gw.log('Backend error: %s' % e)
                        time.sleep(1)
                else:
                    gw.log('Too soon: last_log_time %s' % last_log_time)
    if is_button_pressed():
        disp.println("Opening")
        set_lock(True)
        slack.set_status(":ladeport: BarnDoor: Unlocking")
        gw.log('Unlocked')
        time.sleep(10)
        disp.println("Closing")
        set_lock(False)
        gw.log('Locked')
    if time.time() - last_gw_ping > PING_INTERVAL:
        status = {
            'Barn door': 'closed' if is_door_closed() else 'open'
        }
        gw.ping(status)
        last_gw_ping = time.time()
