import requests, json, os

class RestClient:

    URL = 'https://panopticon.hal9k.dk/api/v1/'
    TOKEN = os.environ['ACS_TOKEN']
    
    def check_card(self, card_id):
        data = '{ "api_token": "%s", "card_id": "%s" }' % (self.TOKEN, card_id)
        response = requests.post(self.URL + 'permissions', data=data, headers={"Content-Type": "application/json"}, verify=False)
        if response.status_code != 200:
            return { 'id': 0 }
        return response.json()

    def log(self, id, msg):
        if id:
            data = '{ "api_token": "%s", "log": { "user_id": %d, "message": "%s" } }' % (self.TOKEN, id, msg)
        else:
            data = '{ "api_token": "%s", "log": { "message": "%s" } }' % (self.TOKEN, msg)
        response = requests.post(self.URL + 'logs', data=data, headers={"Content-Type": "application/json"}, verify=False)
        return response.status_code == 200
        
if __name__ == "__main__":
    r = RestClient()
    cc = r.check_card("0000BB96C5")
    print(cc)
    print(r.log(cc['id'], 'Dummy log entry'))
