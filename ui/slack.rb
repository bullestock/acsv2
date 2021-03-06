require 'json'
require 'rest-client'

class Slack
  def initialize(active, testing)
    @active = active
    @testing = testing
    @token = File.read('slack-token')
    @last_status = ''
  end

  def announce_open()
    set_status(':tada: The space is now open!', true)
  end
  
  def announce_closed()
    set_status(':sad_panda2: The space is no longer open', true)
  end
  
  def set_status(status, include_general = false)
    if status != @last_status
      send_message(status, include_general)
    end
  end

  def get_status()
    @last_status
  end
  
  def send_message(msg, include_general = false)
    puts "SLACK: #{msg}"
    if !@active
      return
    end
    @last_status = msg
    send_to_channel(@testing ? "testing" : "monitoring", msg)
    if include_general && !@testing
      send_to_channel("general", msg)
    end
  end

  def send_to_channel(channel, msg)
    uri = URI.parse("https://slack.com/api/chat.postMessage")
    request = Net::HTTP::Post.new(uri)
    request.content_type = "application/json"
    request["Authorization"] = "Bearer #{@token}"
    body = { channel: channel, icon_emoji: ":panopticon:", parse: "full", "text": msg }
    request.body = JSON.generate(body)
    req_options = {
      use_ssl: uri.scheme == "https",
    }
    response = Net::HTTP.start(uri.hostname, uri.port, req_options) do |http|
      http.request(request)
    end
  end
end # end Slack

if $PROGRAM_NAME == __FILE__
  s = Slack.new(true, true)
  s.send_message(':testcard: This is a test')
end 
