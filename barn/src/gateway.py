import requests, json, os
from datetime import datetime

class Gateway:
    HEARTBEAT_URL = 'https://acsgateway.hal9k.dk/acsheartbeat'
    LOG_URL = "https://acsgateway.hal9k.dk/bacslog";
    TOKEN = os.environ['GW_TOKEN']

    def __init__(self, ident):
        self.ident = ident
    
    def ping(self, status=None):
        data = {}
        data['token'] = self.TOKEN
        data['ident'] = self.ident
        if status:
            data['status'] = status
        try:
            response = requests.post(self.HEARTBEAT_URL, data=json.dumps(data),
                                     headers={"Content-Type": "application/json", "Accept": "application/json"})
            if response.status_code != 200:
                print("Bad gateway heartbeat response: %s" % response)
        except:
            print("Exception connecting to GW")

    def log(self, msg):
        data = {}
        data['token'] = self.TOKEN
        data['ident'] = self.ident
        data['timestamp'] = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        data['text'] = msg;
        try:
            response = requests.post(self.LOG_URL, data=json.dumps(data),
                                     headers={"Content-Type": "application/json", "Accept": "application/json"})
            if response.status_code != 200:
                print("Bad gateway log response: %s" % response)
        except:
            print("Exception connecting to GW")

if __name__ == "__main__":
    gw = Gateway()
    gw.ping()
