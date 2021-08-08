import json

class Slack:
    def __init__(self):
        with open('slack-token', 'r') as file:
            self.token = file.read()
        self.last_status = ''

    def set_status(self, status):
        if status != self.last_status:
            send_message(status)
            self.last_status = status

    def get_status(self):
        return self.last_status
  
    def send_message(self, msg):
        print(f"SLACK: {msg}")
        # uri = URI.parse("https://slack.com/api/chat.postMessage")
        # request = Net::HTTP::Post.new(uri)
        # request.content_type = "application/json"
        # request["Authorization"] = "Bearer #{self.token}"
        # body = { channel: "monitoring", icon_emoji: ":panopticon:", parse: "full", "text": msg }
        # request.body = JSON.generate(body)
        # req_options = {
        #     use_ssl: uri.scheme == "https",
        # }
        # response = Net::HTTP.start(uri.hostname, uri.port, req_options) do |http|
        # http.request(request)
