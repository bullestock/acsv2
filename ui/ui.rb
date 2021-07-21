#!/usr/bin/ruby

require 'optparse'
require 'rest-client'
require 'serialport'
require 'tzinfo'
require './utils.rb'
require './cardreader.rb'

$stdout.sync = true

VERSION = '1.1.0'

HOST = 'https://127.0.0.1'

LED_ENTER = 'P250R8SGN'
LED_NO_ENTRY = 'P100R30SRN'
LED_WAIT = 'P10R0SGNN'
LED_ERROR = 'P5R10SGX10NX100RX100N'
# Slow brief green blink
LED_READY = 'P200R10SG'
# Constant green
LED_OPEN = 'P200R0SG'
LED_CLOSING = 'P5R0SGX10NX100R'
LED_LOW_INTEN = 'I20'
LED_MED_INTEN = 'I50'
LED_HIGH_INTEN = 'I100'

SOUND_UNCALIBRATED = 'S500 500'
SOUND_CANNOT_LOCK = 'S2500 100'
SOUND_LOCK_FAULTY1 = 'S800 200'
SOUND_LOCK_FAULTY2 = 'S1500 150'

# How many seconds green key must be held down to activate timed unlock
UNLOCK_KEY_TIME = 0.1
# How many seconds green key must be held down to activate Thursday mode
THURSDAY_KEY_TIME = 2

# How long to keep the door open after valid card is presented
ENTER_TIME_SECS = 20

TEMP_STATUS_SHOWN_FOR = 10

UNLOCK_PERIOD_S = 15*60
UNLOCK_WARN_S = 5*60

MANUAL_WARN_SECS = 5*60

# Max line length for small font
MAX_LINE_LEN_S = 40

# Max lines of large font text
NOF_TEXT_LINES = 5

SLACK_IMPORTANT = 'Hey @torsten, '

$q = Queue.new
$api_key = File.read('apikey.txt').strip()

$tz = TZInfo::Timezone.get('Europe/Copenhagen')

log_thread = Thread.new do
  puts "Thread start"
  while true
    e = $q.pop
    rest_start = Time.now
    begin
      url = "#{HOST}/api/v1/logs"
      response = RestClient::Request.execute(method: :post,
                                             url: url,
                                             timeout: 60,
                                             payload: { api_token: $api_key,
                                                        log: {
                                                          user_id: e["id"],
                                                          message: e["msg"]
                                                        }
                                                      }.to_json(),
                                             headers: {
                                               'Content-Type': 'application/json',
                                               'Accept': 'application/json'
                                             },
					     :verify_ssl => false)
      puts("log_thread: Got server reply in #{Time.now - rest_start} s")
    rescue Exception => e  
      puts "log_thread: #{e.class} Failed to connect to server"
    end
  end
end

class Ui
  # Display lines for lock status
  STATUS_1 = 2
  STATUS_2 = 4

  def initialize(port, lock)
    @color_map = [
      'white',
      'blue',
      'green',
      'red',
      'navy',
      'darkgreen',
      'darkcyan',
      'cyan',
      'maroon',
      'olive',
      'gray',
      'grey',
      'magenta',
      'orange',
      'yellow'
    ]
    # locked -> unlocked
    # unlocked -> locked
    # locked -> manual
    # unlocked -> manual
    @desired_lock_state = :unknown
    @actual_lock_state = :unknown
    @manual_lock_state = nil
    @manual_mode_at = nil
    @manual_mode_unlocked = false
    @override_manual = false
    @last_time = ''
    @green_pressed_at = nil
    @unlocked_at = nil
    @reader = nil
    @slack = nil
    @who = nil
    @text_lines = Array.new(NOF_TEXT_LINES)
    @text_colour = ''
    @temp_lines = Array.new(NOF_TEXT_LINES)
    @temp_colour = ''
    @temp_status_at = nil
    @temp_status_set = false
    # When to automatically lock again
    @lock_time = nil
    # Whether to show remaining time on display
    @advertise_remaining_time = false
    # One-shot function to call after unlocking
    @after_unlock_fn = nil
    @in_thursday_mode = false
    @complained_on_slack = nil
    @port = port
    @lock = lock
    @port.flush_input
    if @lock
      @lock.flush_input
    end
  end

  def set_status(text, colour)
    #puts("set_status: #{text}")
    old_lines = @text_lines
    @text_lines = do_set_status(text)
    @text_colour = colour
    if !@temp_status_set && @text_lines != old_lines
      write_status()
    end
  end

  def write_status()
    line_no = 0
    @text_lines.each do |line|
      write(true, true, line_no, line, @text_colour)
      line_no = line_no + 1
    end
  end

  def set_temp_status(text, colour = 'white')
    @temp_lines = do_set_status(text)
    @temp_status_colour = colour
    @temp_status_at = Time.now
    @temp_status_set = true
    line_no = 0
    @temp_lines.each do |line|
      write(true, true, line_no, line, colour)
      line_no = line_no + 1
    end
  end

  def do_set_status(text)
    texts = Array(text)
    text_lines = Array.new(NOF_TEXT_LINES)
    case texts.size
    when 0
      return text_lines
    when 1
      text_lines[NOF_TEXT_LINES/2] = texts[0]
    when 2
      text_lines = Array.new(NOF_TEXT_LINES)
      text_lines[NOF_TEXT_LINES/2 - 1] = texts[0]
      text_lines[NOF_TEXT_LINES/2 + 1] = texts[1]
    when 3
      text_lines = Array.new(NOF_TEXT_LINES)
      text_lines[NOF_TEXT_LINES/2 - 1] = texts[0]
      text_lines[NOF_TEXT_LINES/2] = texts[1]
      text_lines[NOF_TEXT_LINES/2 + 1] = texts[2]
    else
      puts("ERROR: #{texts.size} lines not handled")
    end
    return text_lines
  end

  def phase2init()
    clear()
    set_status('Locking', 'orange')
    resp = lock_send_and_wait("set_verbosity 0")
    if !resp[0]
      lock_is_faulty(resp[1])
    end
    resp = lock_send_and_wait("lock")
    if !resp[0]
      if resp[1].include? "not calibrated"
        @reader.send(SOUND_UNCALIBRATED)
        set_status('CALIBRATING', 'red')
        puts "Calibrating lock"
        resp = lock_send_and_wait("calibrate")
        if !resp[0]
          lock_is_faulty(resp[1])
        end
        clear()
      end
    end
    @actual_lock_state = @desired_lock_state = :locked
  end

  def set_reader(reader)
    @reader = reader
  end

  def lock_is_faulty(reply)
    clear()
    write(true, false, 0, 'FATAL ERROR:', 'red')
    write(true, false, 2, 'LOCK REPLY:', 'red')
    write(false, false, 5, reply.strip(), 'red')
    s = "Fatal error: lock said #{reply}"
    puts s
    @slack.set_status(s)
    for i in 1..10
      @reader.send(SOUND_LOCK_FAULTY1)
      sleep(0.5)
      @reader.send(SOUND_LOCK_FAULTY2)
      sleep(0.8)
    end
    Process.exit    
  end
  
  def set_slack(slack)
    @slack = slack
  end
  
  def clear()
    send_and_wait("C")
  end

  def clear_line(large, line)
    send_and_wait(sprintf("#{large ? 'E' :'e'}%02d", line))
  end

  def write(large, erase, line, text, col = 'white')
    col_idx = @color_map.find_index(col)
    s = sprintf("#{large ? 'T' :'t'}%02d%02d%s%s",
                line, col_idx, erase ? '1' : '0', text)
    send_and_wait(s)
  end

  def unlock(who)
    if @desired_lock_state == :unlocked
      return
    end
    @who = who
    @desired_lock_state = :unlocked
    @lock_time = Time.now + ENTER_TIME_SECS
    @advertise_remaining_time = false
    @after_unlock_fn = lambda {
      set_temp_status(['Enter', @who], 'blue')
      @lock_time = Time.now + ENTER_TIME_SECS
    }
  end

  def wait_response(s)
    reply = ''
    while true
      c = @port.getc
      if c
        if c.ord == 13
          next
        end
        if c.ord == 10 && !reply.empty?
          break
        end
        reply = reply + c
      end
    end
    #puts "Reply: #{reply}"
    if reply != "OK #{s[0]}"
      puts "ERROR: Expected 'OK #{s[0]}', got '#{reply.inspect}' (in response to #{s})"
      Process.exit()
    end
  end

  def lock_wait_response(cmd)
    # Skip echo
    while true
      c = @lock.getc
      if c.ord == 10
        break
      end
    end
    reply = ''
    while true
      c = @lock.getc
      if c
        if c.ord == 13
          next
        end
        if c.ord == 10 && !reply.empty?
          break
        end
        reply = reply + c
      end
    end
    #puts "Lock reply: #{reply}"
    if reply[0..1] != "OK"
      puts "ERROR: Expected 'OK', got '#{reply.inspect}' (in response to #{cmd})"
      return [ false, reply ]
    end
    return [ true, reply ]
  end

  def send_and_wait(s)
    #puts("Sending #{s}")
    @port.flush_input()
    @port.puts(s)
    wait_response(s)
  end

  def lock_send_and_wait(s)
    #puts("Lock: Sending #{s}")
    @lock.flush_input()
    @lock.puts(s)
    return lock_wait_response(s)
  end

  def read_keys()
    @port.flush_input()
    @port.puts("S")
    reply = ''
    while true
      c = @port.getc
      if c
        if c.ord == 13
          next
        end
        if c.ord == 10 && !reply.empty?
          break
        end
        reply = reply + c
      end
    end
    #puts "Reply: #{reply}"
    if reply[0] != "S"
      puts "ERROR: Expected 'Sxx', got '#{reply.inspect}'"
      Process.exit()
    end
    return reply[1] == '1', reply[2] == '1', reply[3] == '1', reply[4] == '1'
  end

  def get_lock_status()
    resp = lock_send_and_wait('status')
    if !resp[0]
      puts("ERROR: Could not get status from lock: #{resp[1]}")
      return
    end
    parts = resp[1].split(' ')
    if parts.size != 4
      puts("ERROR: Bad reply from lock: size is #{parts.size}")
      @slack.set_status("Lock status is unknown: Got reply '#{resp[1]}'")
      return
    end
    status = resp[1].split(' ')[2]
    #puts("Lock status #{status}")
    case status
    when 'unknown'
      puts("ERROR: Lock status is unknown")
      @slack.set_status("Lock status is unknown")
    when 'locked'
      @actual_lock_state = :locked
      @manual_lock_state = nil
    when 'unlocked'
      @actual_lock_state = :unlocked
      @manual_lock_state = nil
    when /manual/
      @actual_lock_state = :manual
      if status.start_with? 'locked'
        @manual_lock_state = :locked
      elsif status.start_with? 'unlocked'
        @manual_lock_state = :unlocked
      else
        @manual_lock_state = :unknown
      end
    else
      puts("ERROR: Lock status is '#{status}'")
      @slack.set_status("Lock status is '#{status}', how did that happen?")
    end
    #puts("Actual lock status #{@actual_lock_state}")
  end

  def check_should_lock()
    # Check if door is unlocked and enter time has elapsed
    if @desired_lock_state == :unlocked &&
       @lock_time &&
       Time.now >= @lock_time
      puts "Time elapsed, locking again"
      return_to_auto()
      @desired_lock_state = :locked
    end

    # Check if Thursday mode has expired
    if @desired_lock_state == :unlocked
      if @in_thursday_mode && !is_it_thursday?
        puts "Locking, no longer Thursday"
        return_to_auto()
        @in_thursday_mode = false
        @desired_lock_state = :locked
      end
    end
  end

  def synchronize_lock_state()
    if @override_manual
      puts 'Exit manual lock mode'
      @actual_lock_state = :unknown
      @override_manual = false
    end
    if @actual_lock_state == :manual
      set_status('Manual mode', 'cyan')
      # We are in manual override - check duration
      if !@manual_mode_at
        # Enter manual mode
        @manual_mode_at = Time.now
        @manual_mode_unlocked = true
      end
      if @manual_lock_state != :locked
        @manual_mode_unlocked = true
        if Time.now - @manual_mode_at > MANUAL_WARN_SECS
          # We have been in manual mode for more than MANUAL_WARN_SECS, and are still not locked
          @slack.set_status("Lock is in manual mode since #{$tz.utc_to_local(@manual_mode_at).to_s()[0..19]}")
        end
      else
        # Manually locked
        if @manual_mode_unlocked
          @slack.set_status("Lock has been manually locked")
          @manual_mode_unlocked = false
          set_status(['Manual mode', '(locked)'], 'cyan')
        end
      end
    else
      if @manual_mode_at
        @slack.set_status("Lock has returned to automatic mode")
      end
      @manual_mode_at = nil
      what = ''
      do_clear = false
      case @desired_lock_state
      when :unlocked
        callback = nil
        if @actual_lock_state == :locked
          callback = @after_unlock_fn
          set_status('Unlocking', 'blue')
        end
        resp = lock_send_and_wait("unlock")
        if callback
          callback.call
        end
        what = 'UNLOCK'
      when :locked
        if @actual_lock_state == :unlocked
          set_status('Locking', 'orange')
          do_clear = true
        end
        resp = lock_send_and_wait("lock")
        what = 'LOCK'
      end
      @after_unlock_fn = nil
      if resp[0]
        if do_clear
          set_status('', 'blue')
        end
        if @complained_on_slack
          @slack.set_status("Door is locked")
          @complained_on_slack = false
        end
      else
        clear()
        puts("ERROR: Cannot #{what}: '#{resp[1]}'")
        write(true, false, 0, 'ERROR:', 'red')
        write(true, false, 2, "CANNOT #{what}", 'red')
        cleaned = resp[1].strip()
        line1 = cleaned[0..MAX_LINE_LEN_S-1]
        write(false, false, 7, line1, 'red')
        if cleaned.size > MAX_LINE_LEN_S
          write(false, false, 8, cleaned[MAX_LINE_LEN_S..-1], 'red')
        end
        for i in 0..15
          @reader.send(SOUND_CANNOT_LOCK)
          sleep(0.3)
        end
        @slack.set_status("#{SLACK_IMPORTANT} I could not #{what.downcase} the door")
        @complained_on_slack = true
      end
    end
  end

  # Exit manual lock state
  def return_to_auto()
    if @manual_lock_state
      @manual_lock_state = nil
      @override_manual = true
    end
  end
  
  def check_buttons()
    green, white, red, leave = read_keys()
    if red
      puts "Red pressed at #{Time.now}"
      # Lock
      return_to_auto()
      if @desired_lock_state != :locked
        @desired_lock_state = :locked
        @unlocked_at = nil
        @reader.add_log(nil, 'Door locked')
      end
    elsif green
      puts "Green pressed"
      return_to_auto()
      # Unlock for UNLOCK_PERIOD_S
      if @desired_lock_state != :unlocked
        @desired_lock_state = :unlocked
        @lock_time = Time.now + UNLOCK_PERIOD_S
        @advertise_remaining_time = true
        @reader.add_log(nil, "Door unlocked for #{UNLOCK_PERIOD_S} s")
        puts("Door unlocked, will lock again at #{@lock_time}")
      end
    elsif white
      puts "White pressed"
      # Enter Thursday mode
      if is_it_thursday?
        return_to_auto()
        @desired_lock_state = :unlocked
        @lock_time = nil
        @advertise_remaining_time = false
        @in_thursday_mode = true
        @reader.add_log(nil, 'Enter Thursday mode')
      else
        set_temp_status(['It is not', 'Thursday yet'])
      end
    elsif leave
      puts "Leave pressed"
      @desired_lock_state = :unlocked
      @lock_time = Time.now + ENTER_TIME_SECS
      @advertise_remaining_time = false
      @after_unlock_fn = lambda {
        set_temp_status(['You', 'may', 'leave'], 'blue')
        @lock_time = Time.now + ENTER_TIME_SECS
      }
    end
  end
  
  def update()
    # Update @actual_lock_state
    get_lock_status()
    
    # Check if it is time to lock again
    check_should_lock()

    # Try to make actual lock state match desired lock state
    synchronize_lock_state()

    case @desired_lock_state
    when :locked
      if !@manual_lock_state
        set_status('Locked', 'orange')
      end
      @reader.advertise_ready()
    when :unlocked
      col = 'green'
      if @advertise_remaining_time
        secs_left = (@lock_time - Time.now).to_i
        mins_left = (secs_left/60.0).ceil
        left_text = ''
        if mins_left > 1
          left_text = "#{mins_left} minutes"
        else
          left_text = "#{secs_left} seconds"
        end
        if secs_left <= UNLOCK_WARN_S
          col = 'orange'
          @reader.warn_closing()
        else
          @reader.advertise_open()
        end
        set_status(['Open for', left_text], col)
      else
        set_status('Open', 'green')
        @reader.advertise_open()
      end
    else
      clear()
      write(true, false, 0, 'FATAL ERROR:', 'red')
      write(false, false, 4, 'UNKNOWN LOCK STATE', 'red')
      s = "Fatal error: Unknown desired lock state '#{@desired_lock_state}'"
      puts s
      @slack.set_status(s)
      Process.exit
    end

    if @temp_status_set
      shown_for = Time.now - @temp_status_at
      if shown_for > TEMP_STATUS_SHOWN_FOR
        @temp_status_set = false
        puts("Clear temp status")
        clear()
        write_status()
      end
    end

    check_buttons()
    
    # Time display
    ct = $tz.utc_to_local(Time.now).strftime("%H:%M")
    if ct != @last_time
      send_and_wait("c#{ct}\n")
      @last_time = ct
      @reader.send(get_led_inten_cmd())
    end
  end
end


class Slack
  def initialize()
    @token = File.read('slack-token')
    @last_status = ''
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

slack = Slack.new()

puts "Find ports"
ports = find_ports()
puts "Found ports"
if !ports['ui']
  s = "Fatal error: No UI found"
  puts s
  @slack.set_status(s)
  Process.exit
end

if !ports['lock']
  s = "Fatal error: No lock found"
  puts s
  #@slack.set_status(s)
  ui = Ui.new(ports['ui'], nil)
  ui.set_status(['FATAL ERROR', 'No lock found'], 'red')
  Process.exit
end

ui = Ui.new(ports['ui'], ports['lock'])
ui.clear()

if !ports['reader']
  ui.write(true, false, 0, 'FATAL ERROR:', 'red')
  ui.write(false, false, 4, 'NO READER FOUND', 'red')
  s = "Fatal error: No card reader found"
  puts s
  @slack.set_status(s)
  Process.exit
end

ui.set_slack(slack)

reader = CardReader.new(ports['reader'])
reader.set_ui(ui)
ui.set_reader(reader)

ui.phase2init()

puts("----\nReady")
ui.clear()
slack.send_message("ui.rb v#{VERSION} starting")

USE_WDOG = false #true

if USE_WDOG
  wdog = File.open('/dev/watchdog', 'w')
end

while true
  ui.update()
  reader.update($q, $api_key)
  sleep 0.1
  if USE_WDOG
    wdog.ioctl(0x80045705) # WDIOC_KEEPALIVE
  end
end
