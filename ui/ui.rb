#!/usr/bin/ruby
# coding: utf-8

require 'json'
require 'optparse'
require 'rest-client'
require 'serialport'
require 'tzinfo'

require './cardreader.rb'
require './slack.rb'
require './utils.rb'

$stdout.sync = true

VERSION = '1.2.0 BETA'

HOST = 'https://panopticon.hal9k.dk'

LOCK_LOG = 'lock.log'

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
ENTER_TIME_SECS = 30

# How long to wait before locking when door is closed after entering
AUTO_LOCK_S = 60

# How long to wait before locking when door is closed after leaving
LEAVE_TIME_SECS = 5

TEMP_STATUS_SHOWN_FOR = 10

# Time door is unlocked after pressing Green
UNLOCK_PERIOD_S = 15*60
UNLOCK_WARN_S = 5*60

# Time before warning when entering
ENTER_UNLOCKED_WARN_SECS = 5*60

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

  def log(s)
    puts("#{Time.now} #{s}")
  end
  
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
    # @port.flush_input if !$simulate
    # if @lock
    #   @lock.flush_input
    # end
    @state = :initial
    @card_swiped = false
    @who = nil # Name of card owner
    @timeout = nil
    @last_lock_status = @last_door_status = @last_handle_status = nil
    @show_debug = false
    @lock_error_msg = nil
    @sim_lock_state = nil
    @sim_green = @sim_white = @sim_red = @sim_leave = false
    @sim_card_id = nil
    # Simulation initial state
    @sim_door_closed = true
    @sim_handle_raised = true
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
      text_lines[NOF_TEXT_LINES/2 - 1] = texts[0]
      text_lines[NOF_TEXT_LINES/2 + 1] = texts[1]
    when 3
      text_lines[NOF_TEXT_LINES/2 - 1] = texts[0]
      text_lines[NOF_TEXT_LINES/2] = texts[1]
      text_lines[NOF_TEXT_LINES/2 + 1] = texts[2]
    else
      if texts.size > NOF_TEXT_LINES
        puts("ERROR: #{texts.size} lines not handled")
        return text_lines
      end
      i = 0
      texts.each do |t|
        text_lines[i] = t
        i = i + 1
      end
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
    write(true, false, 4, disp2, 'red')
    s = "Fatal error: #{msg}"
    log(s)
    @slack.set_status(s)
  end

  def lock_is_faulty(reply)
    fatal_error('LOCK REPLY:', reply.strip(), "lock said #{reply}")
    for i in 1..10
      @reader.set_sound(SOUND_LOCK_FAULTY1)
      sleep(0.5)
      @reader.set_sound(SOUND_LOCK_FAULTY2)
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
    if $simulate
      if line == 0
        puts("====================")
      end
      puts("DISP:#{line} #{text}")
      if line == NOF_TEXT_LINES-1
        puts("====================")
      end
      return
    end
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
      log("ERROR: Expected 'OK #{s[0]}', got '#{reply.inspect}' (in response to #{s})")
      Process.exit()
    end
  end

  # Return success, reply
  def lock_wait_response(cmd)
    # Skip echo
    while true
      c = @lock.getc
      if c
        #puts "Got #{c}"
        if c.ord == 10
          break
        end
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
        if @show_debug
          log(reply)
        end
        File.open(LOCK_LOG,"a") do |f|
          f.puts "#{Time.now}   #{reply}"
        end
      else
        break
      end
    end
    #puts "Lock reply: #{reply}"
    if reply[0..1] != "OK"
      log("ERROR: Expected 'OK', got '#{reply.inspect}' (in response to #{cmd})")
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
    #@lock.flush_input()
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
    if !@sim_door_closed
      @sim_handle_raised = false
    end
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
      log("ERROR: Expected 'Sxx', got '#{reply.inspect}'")
      Process.exit()
    end
    return reply[1] == '1', reply[2] == '1', reply[3] == '1', reply[4] == '1'
  end

  # lock_status, door_status, handle_status
  # locked/unlocked, open/closed, raised/lowered
  def get_lock_status()
    ok, reply = lock_send_and_wait('status')
    if !ok
      log("ERROR: Could not get status from lock: #{reply}")
      @lock_error_msg = reply
      return
    end
    @lock_error_msg = nil
    # Format: "OK: status locked open lowered"
    parts = reply.split(' ')
    if parts.size != 5
      log("ERROR: Bad reply from lock: size is #{parts.size}")
      @slack.set_status("Lock status is unknown: Got reply '#{reply}'")
      return
    end
    status = reply.split(' ')[2]
    door_status = reply.split(' ')[3]
    handle_status = reply.split(' ')[4]
    return status, door_status, handle_status
  end

  def get_lock_error
    return @lock_error_msg
  end
  
  # Try to make the lock enter the specified state; return true if success.
  # Legal states:
  # :locked
  # :unlocked
  def ensure_lock_state(actual_lock_state, desired_lock_state)
    if actual_lock_state == 'unknown'
      @reader.set_sound(SOUND_UNCALIBRATED) if !$simulate
      set_status('CALIBRATING', 'red')
      msg = "Calibrating lock"
      log(msg)
      @slack.set_status(msg)
      ok, reply = lock_send_and_wait("calibrate")
      if !ok
        lock_is_faulty(reply)
        return false
      end
      set_status('CALIBRATED', 'blue')
      actual_lock_state = 'unlocked'
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
        log("ERROR: Cannot lock the door: '#{reply}'")
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
        log("ERROR: Cannot unlock the door: '#{reply}'")
        #!! error handling
      end
      return false
    else
      fatal_error('BAD LOCK STATE:', actual_lock_state, "unhandled lock state: #{actual_lock_state}")
      Process.exit(1)
    end
  end

  # Called asynchronously by CardReader when a valid card has been swiped
  def unlock(who)
    @card_swiped = true
    @who = who
  end

  def fatal_lock_error(msg)
    if !$simulate
      msg = "#{msg}: #{get_lock_error()}"
    end
    fatal_error('COULD NOT', 'LOCK DOOR', msg)
    Process.exit    
  end
  
  def update()
    #!! TODO:
    # - set timeout when needed
    # - clear timeout when needed
    # - provide feedback when changing state
    lock_status, door_status, handle_status = get_lock_status()
    if lock_status != @last_lock_status || door_status != @last_door_status || handle_status != @last_handle_status
      log("Lock status #{lock_status} #{door_status} #{handle_status}")
      @last_lock_status = lock_status
      @last_door_status = door_status
      @last_handle_status =  handle_status
    end
    green, white, red, leave = read_keys()
    old_state = @state
    timeout_dur = nil
    case @state
    when :initial
      if door_status == 'open'
        log("Door is open, wait")
        @state = :wait_for_close
      elsif handle_status == 'lowered'
        log("Handle is not raised, wait")
        @state = :wait_for_close
      else
        @state = :locking
      end
    when :alert_unlocked
      set_status(['Please', 'close the', 'door and raise', 'the handle'], 'red')
      if @timeout && (Time.now() >= @timeout)
        #!! complain
        timeout_dur = UNLOCKED_ALERT_INTERVAL_S
      end
      if green || door_status == 'open'
        @state = :unlocked
        timeout_dur = UNLOCK_PERIOD_S
      end
    when :locked
      set_status('Locked', 'orange')
      if white
        if is_it_thursday?
          @state = :opening
        else
          set_temp_status(['It is not', 'Thursday yet'])
        end
      elsif green
        log("Green pressed at #{Time.now}")
        @state = :timed_unlocking
        timeout_dur = UNLOCK_PERIOD_S
      elsif @card_swiped
        @card_swiped = false
        @state = :unlocking
        timeout_dur = ENTER_TIME_SECS
      elsif leave
        @state = :wait_for_leave_unlock
      end
    when :locking
      set_status('Locking', 'orange')
      if ensure_lock_state(lock_status, :locked)
        @state = :locked
      else
        fatal_lock_error("could not lock the door")
      end
    when :open
      set_status('Open', 'green')
      if !is_it_thursday?
        @state = :unlocked
      elsif red
        if door_status != 'closed'
          set_temp_status(['Please lock', 'the door'], 'yellow')
        elsif handle_status != 'raised'
          set_temp_status(['Please raise', 'the handle'], 'yellow')
        else
          @state = :locking
        end
      end
    when :opening
      set_status('Unlocking', 'blue')
      if ensure_lock_state(lock_status, :unlocked)
        @state = :open
      else
        fatal_lock_error("could not unlock the door")
      end
    when :timed_unlock
      if red || (@timeout && Time.now() >= @timeout)
        @timeout = nil
        if door_status != 'closed' || handle_status != 'raised'
          set_status(['Please', 'close the', 'door and raise', 'the handle'], 'red')
        else
          @state = :locking
        end
      elsif @timeout
        secs_left = (@timeout - Time.now()).to_i
        if secs_left <= UNLOCK_WARN_S
          mins_left = (secs_left/60.0).ceil
          #puts "Left: #{mins_left}m #{secs_left}s"
          if mins_left > 1
            s2 = "#{mins_left} minutes"
          else
            s2 = "#{secs_left} seconds"
          end
          col = 'orange'
          @reader.warn_closing() if !$simulate
          set_status(['Open for', s2], 'orange')
        else            
          set_status('Open', 'green')
        end
      end
      if !@timeout
        if door_status != 'closed' || handle_status != 'raised'
          set_status(['Please', 'close the', 'door and raise', 'the handle'], 'red')
          if green
            @state = :timed_unlock
            timeout_dur = UNLOCK_PERIOD_S
          end
        else
          @state = :locking
        end
      end
      if leave
        @state = :wait_for_leave
        timeout_dur = LEAVE_TIME_SECS
      end
    when :timed_unlocking
      set_status('Unlocking', 'blue')
      if ensure_lock_state(lock_status, :unlocked)
        @state = :timed_unlock
        timeout_dur = UNLOCK_PERIOD_S
      else
        fatal_lock_error("could not unlock the door")
      end
    when :unlocked
      set_status('Unlocked', 'orange')
      if door_status == 'opened'
        @state = :wait_for_enter
      elsif door_status == 'closed'
        @state = :wait_for_handle
      elsif leave
        @state = :wait_for_leave
        timeout_dur = LEAVE_TIME_SECS
      elsif @timeout && (Time.now >= @timeout)
        @state = :alert_unlocked
        @timeout = Time.now()
      end
    when :unlocking
      set_status('Unlocking', 'blue')
      if ensure_lock_state(lock_status, :unlocked)
        @state = :wait_for_open
      else
        fatal_lock_error("could not unlock the door")
      end
    when :wait_for_close
      if green
        @state = :unlocked
      elsif door_status == 'open'
        set_status(['Please close', 'the door', 'and raise', 'the handle'], 'red')
      else
        set_status(['Please raise', 'the handle'], 'red')
        if handle_status == 'raised'
          @state = :locking
        end
      end
      if white
        if is_it_thursday?
          @state = :opening
        else
          set_temp_status(['It is not', 'Thursday yet'])
        end
      end
    when :wait_for_enter
      set_status('Enter', 'blue')
      if door_status == 'closed'
        @state = :wait_for_handle
        timeout_dur = ENTER_UNLOCKED_WARN_SECS
      elsif green
        @state = :timed_unlocking
      end
    when :wait_for_handle
      set_status(['Please raise', 'the handle'], 'blue')
      if @timeout && (Time.now() >= @timeout)
        @timeout = nil
        @state = :alert_unlocked
      elsif green
        @state = :timed_unlocking
      elsif door_status == 'open'
        @state = :wait_for_enter
      elsif handle_status == 'raised'
        @state = :wait_for_lock
        timeout_dur = AUTO_LOCK_S
      end
    when :wait_for_leave
      set_status('You may leave', 'blue')
      if @timeout && (Time.now() >= @timeout)
        @timeout = nil
        if door_status != 'closed' || handle_status != 'raised'
          set_status(['Please', 'close the', 'door and raise', 'the handle'], 'red')
        else
          @state = :locking
        end
      elsif door_status == 'open'
        @state = :wait_for_close
      end
    when :wait_for_leave_unlock
      set_status('Unlocking', 'blue')
      if ensure_lock_state(lock_status, :unlocked)
        @state = :wait_for_leave
        timeout_dur = LEAVE_TIME_SECS
      else
        fatal_lock_error("could not unlock the door")
      end
    when :wait_for_lock
      if red || (@timeout && (Time.now() >= @timeout))
        @timeout = nil
      elsif green
        @state = :timed_unlock
        timeout_dur = UNLOCK_PERIOD_S
      elsif @timeout
        secs_left = (@timeout - Time.now()).to_i
        mins_left = (secs_left/60.0).ceil
        if mins_left > 1
          s2 = "#{mins_left} minutes"
        else
          s2 = "#{secs_left} seconds"
        end
        col = 'orange'
        set_status(['Locking in', s2], 'orange')
      end
      if !@timeout
        if door_status != 'closed' || handle_status != 'raised'
          set_status(['Please', 'close the', 'door and raise', 'the handle'], 'red')
        else
          @state = :locking
        end
      end
    when :wait_for_open
      set_status(['Enter', @who], 'blue')
      if @timeout && (Time.now() >= @timeout)
        @timeout = nil
        if door_status != 'closed' || handle_status != 'raised'
          set_status(['Please', 'close the', 'door and raise', 'the handle'], 'red')
        else
          @state = :locking
        end
      elsif door_status == 'open'
        @state = :wait_for_enter
      end
    else
      fatal_error('BAD STATE:', @state, "unhandled state: #{@state}")
      Process.exit(1)
    end
    if @state != old_state
      log("STATE: #{@state}")
    end
    if timeout_dur
      log("Set timeout of #{timeout_dur} s")
      @timeout = Time.now() + timeout_dur
    end
    
    if @temp_status_set
      shown_for = Time.now - @temp_status_at
      if shown_for > TEMP_STATUS_SHOWN_FOR
        @temp_status_set = false
        log("Clear temp status")
        clear()
        write_status()
      end
    end

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

#File.delete(LOCK_LOG) if File.exist?(LOCK_LOG)

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

  puts "Create UI"
  ui = Ui.new(ports['ui'], ports['lock'])
  puts "Created UI"
  ui.clear()
  puts "Cleared UI"

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
      ui.unlock("Test Testersson")
    end
  end
end
