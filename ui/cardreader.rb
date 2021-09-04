require 'json'
require 'rest-client'

class CardReader
  def initialize(port)
    @port = port
    @port.flush_input
    @last_card = ''
    @last_card_read_at = Time.now()
    @last_card_seen_at = Time.now()
    @last_led_cmd = nil
  end

  def set_ui(ui)
    @ui = ui
  end

  def send(s)
    @port.flush_input
    #puts "Send(#{Time.now}): #{s}"
    @port.puts(s)
    @port.flush_output
    begin
      line = @port.gets
    end while !line || line.empty?
    line.strip!
    #puts "Reply: #{line}"
    if line != "OK"
      puts "ERROR: Expected 'OK', got '#{line}' (in response to #{s})"
      Process.exit()
    end
  end

  def set_sound(s)
    # Sound disabled for now
  end
  
  def warn_closing()
    set_led(LED_CLOSING)
  end
  
  def advertise_open()
    set_led(LED_OPEN)
  end

  def advertise_ready()
    set_led(LED_READY)
  end

  def check_permission(id, api_key)
    db_start = Time.now
    allowed = false
    error = false
    who = ''
    user_id = nil
    rest_start = Time.now
    begin
      url = "#{HOST}/api/v1/permissions"
      json = { 'api_token': api_key,
               'card_id': id
             }.to_json()
      response = RestClient::Request.execute(method: :post,
                                             url: url,
                                             timeout: 60,
                                             payload: json,
                                             headers: {
                                               'Content-Type': 'application/json',
                                               'Accept': 'application/json'
                                             },
					     :verify_ssl => false)
      puts("Got server reply in #{Time.now - rest_start} s")
    rescue RestClient::NotFound => e  
      puts "Card not found"
      return false, false, nil, nil
    rescue Exception => e  
      puts "check_permission: Exception #{e.class}"
      return false, true, nil, nil
    end
    if response.code == 200
      json_response = JSON.parse(response)
      puts json_response
      allowed = json_response['allowed']
      who = json_response['name']
      user_id = json_response['id']
    else
      error = true
    end
    return allowed, error, who, user_id
  end
  
  def add_log(q, id, msg)
    q << { 'id' => id, 'msg' => msg }
  end
  
  def add_unknown_card(api_key, id)
    rest_start = Time.now
    error = false
    begin
      url = "#{HOST}/api/v1/unknown_cards"
      response = RestClient::Request.execute(method: :post,
                                             url: url,
                                             timeout: 60,
                                             payload: { api_token: api_key,
                                                        card_id: id
                                                      }.to_json(),
                                             headers: {
                                               'Content-Type': 'application/json',
                                               'Accept': 'application/json'
                                             },
					     :verify_ssl => false)
      puts("Got server reply in #{Time.now - rest_start} s")
    rescue Exception => e  
      puts "unknown_cards: Exception #{e.class}"
      error = true
    end
    return !error
  end

  def set_led(cmd)
    if cmd == @last_led_cmd
      return
    end
    @last_led_cmd = cmd
    send(cmd)
  end

  def update(q, api_key)
    if Time.now - @last_card_read_at < 1
      return
    end
    @last_card_read_at = Time.now
    @port.flush_input
    @port.puts("C")
    #puts "Send(#{Time.now}): C"
    begin
      line = @port.gets
    end while !line || line.empty?
    line.strip!
    #puts "Reply: #{line}"
    if !line || line.empty? || line[0..1] != "ID"
      puts "ERROR: Expected 'IDxxxxxx', got '#{line}' (in response to C)"
      return
    end
    line = line[2..-1]
    if !line.empty? && line.length != 10
      puts "Invalid card ID: #{line}"
      line = ''
    end
    now = Time.now()
    if !line.empty? && ((line != @last_card) || (now - @last_card_seen_at > 5))
      puts "Card ID: #{line}"
      @last_card = line
      @last_card_seen_at = now
      # Let user know we are doing something
      set_led(LED_WAIT)
      allowed, error, who, user_id = check_permission(@last_card, api_key)
      if error
        set_led(LED_ERROR)
      else
        if allowed == true
          @ui.unlock(who)
          add_log(q, user_id, 'Granted entry')
        elsif allowed == false
          set_led(LED_NO_ENTRY)
          if user_id
            add_log(q, user_id, 'Denied entry')
            @ui.set_temp_status(['Denied entry:', who], 'red')
          else
            add_log(q, user_id, "Denied entry for #{@last_card}")
            add_unknown_card(api_key, @last_card)
            @ui.set_temp_status(['Unknown card', @last_card], 'yellow')
          end
        else
          puts("Impossible! allowed is neither true nor false: #{allowed}")
          set_led(LED_ERROR)
        end
      end
    end
  end

end # end CardReader
