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

TIMEOUT = 10
LOG_TIMEOUT = 5*60
PING_INTERVAL = 5*60

VERSION = "0.1"

def set_lock(on):
    GPIO.output('PA01', on)

def is_button_pressed():
    return GPIO.input('PA06')

def is_door_closed():
    return not GPIO.input('PA03')

disp = Display()

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

gw = Gateway()

last_card_id = None
last_card_time = time.time() - TIMEOUT
last_log_time = time.time() - LOG_TIMEOUT
last_gw_ping = time.time() - PING_INTERVAL

gw.log("%s Started" % datetime.now())
sys.stdout.flush()
slack.set_status(":ladeport: BarnDoor version %s starting" % VERSION)
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
    elif time.time() - last_gw_ping > PING_INTERVAL:
        gw.ping()
        last_gw_ping = time.time()
