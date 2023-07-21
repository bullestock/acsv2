import requests
from datetime import datetime

class Slack:

    def __init__(self, gw):
        file = open('slack-token')
        self.token = file.read().strip()
        self.last_status = None
        self.gw = gw

    def set_status(self, status):
        if status != self.last_status:
            if status:
                self.send_message(status)
            self.last_status = status

    def get_status(self):
        return self.last_status
  
    def send_message(self, msg):
        if self.gw:
            self.gw.log("%s SLACK: %s" % (datetime.now().strftime("%Y-%m-%d %H:%M:%S"), msg))
        else:
            print("%s SLACK: %s" % (datetime.now().strftime("%Y-%m-%d %H:%M:%S"), msg))
        try:
            body = { 'channel': "private-monitoring", 'icon_emoji': ":panopticon:", 'parse': "full", "text": msg }
            headers = {
                'content_type': "application/json",
                "Authorization": "Bearer %s" % self.token
            }
            r = requests.post(url = "https://slack.com/api/chat.postMessage", data = body, headers = headers)
            if self.gw:
                self.gw.log("%s Slack retcode: %d" % (datetime.now().strftime("%Y-%m-%d %H:%M:%S"), r.status_code))
            else:
                print("%s Slack retcode: %d" % (datetime.now().strftime("%Y-%m-%d %H:%M:%S"), r.status_code))
        except Exception as e:
            if self.gw:
                self.gw.log("%s Slack exception: %s" % (datetime.now().strftime("%Y-%m-%d %H:%M:%S"), e))
            else:
                print("%s Slack exception: %s" % (datetime.now(), e))

if __name__ == "__main__":
    s = Slack(None)
    s.send_message("Hej fra laden")
