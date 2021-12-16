require 'json'
require 'rest-client'

class Gateway
  def initialize()
    @token = File.read('gw-token')
  end

  def set_status(status)
    uri = URI.parse("https://acsgateway.hal9k.dk/acsstatus")
    request = Net::HTTP::Post.new(uri)
    request.content_type = "application/json"
    request["Content-Type"] = "application/json"
    request["Accept"] = "application/json"
    body = { token: @token, status: status }
    request.body = JSON.generate(body)
    req_options = {
      use_ssl: uri.scheme == "https",
    }
    response = Net::HTTP.start(uri.hostname, uri.port, req_options) do |http|
      http.request(request)
    end
  end
end

if $PROGRAM_NAME == __FILE__
  gw = Gateway.new
  gw.set_status('Encoder position': 47, handle: 'raised', door: 'open')
end 
