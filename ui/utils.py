import datetime, re, serial, string

def find_ports():
    r = {}
    printable = set(string.printable)
    for p in range(0, 3):
        port = "/dev/ttyUSB#{p}"
        try:
            sp = serial.Serial(port,
                               115200,
                               bytesize = 8,
                               parity = serial.PARITY_NONE,
                               timeout = 0.1)
            if sp:
                print(f"Found #{port}")
                while True:
                    sleep(0.1)
                    print("Send V")
                    sp.write("V\n")
                    sleep(0.1)
                    while True:
                        line = sp.readline()
                        print(f"Got {line}")
                        if line and len(line) > 0:
                            break
                    line = line.strip()
                    reply = ''.join(filter(lambda x: x in printable, reply))
                    print(f"Got #{line} -> #{reply}")
                    if reply == "V":
                        # Echo is on
                        line = sp.readline()
                        reply = ''.join(filter(lambda x: x in printable, reply))
                    if "ACS" in reply:
                        puts(f"Version: #{reply}")
                        if "UI" in reply:
                            r['ui'] = sp
                            version = re.sub(r'.* v ', '', reply)
                            break
                        elif "cardreader" in reply:
                            r['reader'] = sp
                            break
                    elif "Danalock" in reply:
                        r['lock'] = sp
                        break

            sp.reset_input_buffer()
        except:
            print("Exception: #{e}")
            # No port here
    return r

def is_it_thursday():
    global tz
    return (datetime.datetime.now().strftime("%A") == 'Thursday') and (tz.utc_to_local(Time.now).hour >= 15)

def get_led_inten_cmd():
    h = Time.now.hour
    if h < 5 or h > 20:
        return LED_LOW_INTEN

    if h < 8 or h > 16:
        return LED_MED_INTEN

    return LED_HIGH_INTEN
