import requests, json, os

class Gateway:

    URL = 'https://acsgateway.hal9k.dk/acsheartbeat'
    TOKEN = os.environ['GW_TOKEN']
    
    def ping(self):
        data = {}
        data['token'] = self.TOKEN
        try:
            response = requests.post(self.URL, data=json.dumps(data),
                                     headers={"Content-Type": "application/json", "Accept": "application/json"})
            if response.status_code != 200:
                print("Bad gateway response: %s" % response)
        except:
            print("Exception connecting to GW")

if __name__ == "__main__":
    gw = Gateway()
    gw.ping()
