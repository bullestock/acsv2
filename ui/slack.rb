require 'json'

class Slack
  def initialize()
    @token = File.read('slack-token')
    @last_status = ''
  end

  def announce_open()
    set_status(':tada: The space is now open!')
  end
  
  def set_status(status)
    if status != @last_status
      send_message(status)
      @last_status = status
    end
  end

  def get_status()
    @last_status
  end
  
  def send_message(msg)
    puts "SLACK: #{msg}"
    uri = URI.parse("https://slack.com/api/chat.postMessage")
    request = Net::HTTP::Post.new(uri)
    request.content_type = "application/json"
    request["Authorization"] = "Bearer #{@token}"
    body = { channel: "monitoring", icon_emoji: ":panopticon:", parse: "full", "text": msg }
    request.body = JSON.generate(body)
    req_options = {
      use_ssl: uri.scheme == "https",
    }
    response = Net::HTTP.start(uri.hostname, uri.port, req_options) do |http|
      http.request(request)
    end
  end
end # end Slack
