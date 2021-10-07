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
    send(s)
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

  def set_led(cmd)
    if cmd == @last_led_cmd
      return
    end
    @last_led_cmd = cmd
    send(cmd)
  end

  def update()
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
      @ui.swiped(@last_card)
    end
  end

end # end CardReader
