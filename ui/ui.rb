#!/usr/bin/ruby

require 'json'
require 'optparse'
require 'rest-client'
require 'serialport'
require 'tzinfo'

require './cardreader.rb'
require './slack.rb'
require './utils.rb'

$stdout.sync = true

VERSION = '1.1.0 ALPHA'

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

$simulate = false

$q = Queue.new
$api_key = File.read('apikey.txt').strip()

$tz = TZInfo::Timezone.get('Europe/Copenhagen')

if !$simulate
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
end

def getkey
  system('stty raw -echo') # => Raw mode, no echo
  char = (STDIN.read_nonblock(1) rescue nil)
  system('stty -raw echo') # => Reset terminal mode
  return char
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
    @reader = nil
    @slack = nil
    @text_lines = Array.new(NOF_TEXT_LINES)
    @text_colour = ''
    @temp_lines = Array.new(NOF_TEXT_LINES)
    @temp_colour = ''
    @temp_status_at = nil
    @temp_status_set = false
    @complained_on_slack = nil
    @port = port
    @lock = lock
    @port.flush_input if !$simulate
    if @lock
      @lock.flush_input
    end
    # :initial
    # :locked
    # :unlocking
    # :unlocked
    # :alert_unlocked
    # :fatal_error
    # :timed_unlocking
    # :timed_unlock
    # :leaving
    # :locking
    # :wait_for_lock
    # :wait_for_open
    # :wait_for_locking
    # :wait_for_leave
    # :wait_for_close
    @state = :initial
    @sim_lock_state = nil
    @sim_green = @sim_white = @sim_red = @sim_leave = false
    @sim_door_closed = true
    @sim_handle_raised = true
    @sim_card_id = nil
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
    ok, reply = lock_send_and_wait("set_verbosity 1")
    if !ok
      lock_is_faulty(reply)
    end
  end
  
  def set_reader(reader)
    @reader = reader
  end

  def fatal_error(disp1, disp2, msg)
    clear()
    write(true, false, 0, 'FATAL ERROR:', 'red')
    write(true, false, 2, disp1, 'red')
    write(false, false, 5, disp2, 'red')
    s = "Fatal error: #{msg}"
    puts s
    @slack.set_status(s)
  end

  def lock_is_faulty(reply)
    fatal_error('LOCK REPLY:', reply.strip(), "lock said #{reply}")
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

  # Return success, reply
  def lock_wait_response(cmd)
    # Skip echo
    while true
      c = @lock.getc
      if c.ord == 10
        break
      end
    end
    while true
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
      if reply[0..6] == "DEBUG: "
        puts reply
      else
        break
      end
    end
    #puts "Lock reply: #{reply}"
    if reply[0..1] != "OK"
      puts "ERROR: Expected 'OK', got '#{reply.inspect}' (in response to #{cmd})"
      return false, reply
    end
    return true, reply
  end

  def send_and_wait(s)
    if $simulate
      return
    end
    #puts("Sending #{s}")
    @port.flush_input()
    @port.puts(s)
    wait_response(s)
  end

  # Return success, reply
  def lock_send_and_wait(s)
    if $simulate
      case s
      when 'status'
        lock_state = 'unknown'
        if @sim_lock_state
          lock_state = @sim_lock_state
        end
        return true, "OK status #{lock_state} #{@sim_door_closed ? 'closed' : 'open'} #{@sim_handle_raised ? 'raised' : 'lowered'}"
      when 'lock'
        if @sim_lock_state
          @sim_lock_state = 'locked'
        else
          return false, "ERROR: not calibrated"
        end
      when 'unlock'
        @sim_lock_state = 'unlocked'
      when 'calibrate'
        @sim_lock_state = 'locked'
      end
      return true, "OK #{s}"
    end
    #puts("Lock: Sending #{s}")
    @lock.flush_input()
    @lock.puts(s)
    return lock_wait_response(s)
  end

  # For simulation only
  def key_pressed(key)
    case key
    when :green
      @sim_green = true
    when :white
      @sim_white = true
    when :red
      @sim_red = true
    when :leave
      @sim_leave = true
    end
  end

  # For simulation only
  def toggle_door_state()
    @sim_door_closed = !@sim_door_closed
    puts "Door is now #{@sim_door_closed ? 'closed' : 'open'}"
  end
  
  # For simulation only
  def toggle_handle_state()
    @sim_handle_raised = !@sim_handle_raised
    puts "Handle is now #{@sim_handle_raised ? 'raised' : 'lowered'}"
  end

  # For simulation only
  def swipe_card()
    @sim_card_id = ''
    puts "Swiped card #{@sim_card_id}"
  end

  # green, white, red, leave
  def read_keys()
    if $simulate
      keys = @sim_green, @sim_white, @sim_red, @sim_leave
      @sim_green = @sim_white = @sim_red = @sim_leave = false
      return keys
    end
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
    ok, reply = lock_send_and_wait('status')
    if !ok
      puts("ERROR: Could not get status from lock: #{reply}")
      return
    end
    # Format: "OK: status locked open lowered"
    parts = reply.split(' ')
    if parts.size != 5
      puts("ERROR: Bad reply from lock: size is #{parts.size}")
      @slack.set_status("Lock status is unknown: Got reply '#{reply}'")
      return
    end
    status = reply.split(' ')[2]
    door_status = reply.split(' ')[3]
    handle_status = reply.split(' ')[4]
    puts("Lock status #{status} #{door_status} #{handle_status}")
    return status, door_status, handle_status
  end

  def check_buttons()
    green, white, red, leave = read_keys()
    if red
      puts "Red pressed at #{Time.now}"
      # Lock
      #!!
    elsif green
      puts "Green pressed"
      # Unlock for UNLOCK_PERIOD_S
      #!!
    elsif white
      puts "White pressed"
      # Enter Thursday mode
      if is_it_thursday?
        #!!
      else
        set_temp_status(['It is not', 'Thursday yet'])
      end
    elsif leave
      puts "Leave pressed"
      #!!
    end
  end

  # Try to make the lock enter the specified state; return true if success.
  # Legal states:
  # :locked
  # :unlocked
  def ensure_lock_state(actual_lock_state, desired_lock_state)
    if actual_lock_state == 'unknown'
      @reader.send(SOUND_UNCALIBRATED) if !$simulate
      set_status('CALIBRATING', 'red')
      msg = "Calibrating lock"
      puts msg
      @slack.set_status(msg)
      ok, reply = lock_send_and_wait("calibrate")
      if !ok
        lock_is_faulty(reply)
        return false
      end
      actual_lock_state = 'locked'
    end
    case desired_lock_state
    when :locked
      if actual_lock_state == 'locked'
        return true
      end
      if actual_lock_state == 'unlocked'
        ok, reply = lock_send_and_wait('lock')
        if ok
          return true
        end
        puts("ERROR: Cannot lock the door: '#{reply}'")
        #!! error handling
      end
      return false
    when :unlocked
      if actual_lock_state == 'unlocked'
        return true
      end
      if actual_lock_state == 'locked'
        ok, reply = lock_send_and_wait('unlock')
        if ok
          return true
        end
        puts("ERROR: Cannot unlock the door: '#{reply}'")
        #!! error handling
      end
      return false
    else
      fatal_error('BAD LOCK STATE:', actual_lock_state, "unhandled lock state: #{actual_lock_state}")
      Process.exit(1)
    end
  end

  # Called asynchronously by CardReader when a valid card has been swiped
  def unlock()
  end
  
  def update()
    lock_status, door_status, handle_status = get_lock_status()
    case @state
    when :initial
      if door_status == 'open'
        puts "Door is open, wait"
        @state = :wait_for_locking
      elsif handle_status == 'lowered'
        puts "Handle is not raised, wait"
        @state = :wait_for_locking
      else
        if ensure_lock_state(lock_status, :locked)
          @state = :locked
        else
          msg = "could not lock the door"
          if !$simulate
             msg = "#{msg}: #{@lock.get_error()}"
          end
          fatal_error('COULD NOT', 'LOCK DOOR', msg)
          Process.exit    
        end
      end
    when :locked
      set_status('Locked', 'orange')
      # Card swiped: Go to :unlocking
      # Green pressed: Go to :timed_unlock
      # Leave pressed: Go to :leaving
    when :unlocking
    when :unlocked
    when :alert_unlocked
    when :fatal_error
    when :timed_unlocking
    when :timed_unlock
    when :leaving
    when :locking
    when :wait_for_lock
    when :wait_for_open
    when :wait_for_locking
    when :wait_for_leave
    when :wait_for_close
    else
      fatal_error('BAD STATE:', @state, "unhandled state: #{@state}")
      Process.exit(1)
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
    if !$simulate
      ct = $tz.utc_to_local(Time.now).strftime("%H:%M")
      if ct != @last_time
        send_and_wait("c#{ct}\n")
        @last_time = ct
        @reader.send(get_led_inten_cmd())
      end
    end
  end
end

OptionParser.new do |opts|
  opts.banner = "Usage: ui.rb [options]"

  opts.on("-s", "--simulate", "Simulate serial devices") do |n|
    $simulate = true
  end
end.parse!

slack = Slack.new()

slack.send_message("ui.rb v#{VERSION} starting") if !$simulate

if !$simulate
  puts "Find ports"
  ports = find_ports()
  puts "Found ports"
  if !ports['ui']
    s = "Fatal error: No UI found"
    puts s
    slack.set_status(s)
    Process.exit
  end

  if !ports['lock']
    s = "Fatal error: No lock found"
    puts s
    slack.set_status(s)
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
    slack.set_status(s)
    Process.exit
  end
else
  ui = Ui.new(nil, nil)
end

ui.set_slack(slack)

if !$simulate
  reader = CardReader.new(ports['reader'])
  reader.set_ui(ui)
  ui.set_reader(reader)
end

ui.phase2init()

puts("----\nReady")
ui.clear()

while true
  ui.update()
  if !$simulate
    reader.update($q, $api_key)
    sleep 0.1
  else
    sleep 1
    case getkey()
    when 'r'
      ui.key_pressed(:red)
    when 'g'
      ui.key_pressed(:green)
    when 't'
      ui.key_pressed(:white)
    when 'l'
      ui.key_pressed(:leave)
    when 'd'
      ui.toggle_door_state()
    when 'h'
      ui.toggle_handle_state()
    when 'c'
      puts "Card swiped"
      ui.unlock()
    end
  end
end
