def find_ports()
  r = {}
  for p in 0..2
    port = "/dev/ttyUSB#{p}"
    begin
      sp = SerialPort.new(port,
                          { 'baud' => 115200,
                            'data_bits' => 8,
                            'parity' => SerialPort::NONE,
                            'read_timeout' => 100
                          })
      if sp
        puts "Found #{port}"
        begin
          while true
            sleep 0.1
            puts "Send V"
            sp.puts("V")
            sleep 0.1
            begin
              line = sp.gets
              puts "Got #{line}"
            end while !line || line.empty?
            line.strip!
            reply = line.gsub(/[^[:print:]]/i, '')
            puts "Got #{line} -> #{reply}"
            if reply == "V"
              # Echo is on
              line = sp.gets
              reply = line.gsub(/[^[:print:]]/i, '')
            end
            if reply.include? "ACS"
              puts("Version: #{reply}")
              if reply.include? "UI"
                r['ui'] = sp
                version = reply.gsub(/.* v /, '')
                $opensmart = version[0] == '1'
                break
              elsif reply.include? "cardreader"
                r['reader'] = sp
                break
              end
            elsif reply.include? "Danalock"
              r['lock'] = sp
              break
            end
          end
          sp.flush_input
        end
      end
    rescue Exception => e  
      puts "Exception: #{e}"
      # No port here
    end
  end
  return r
end

def is_it_thursday?
  return (Date.today.strftime("%A") == 'Thursday') && ($tz.utc_to_local(Time.now).hour >= 15)
end

def get_led_inten_cmd
  h = Time.now.hour
  if h < 5 || h > 20
    return LED_LOW_INTEN
  end
  if h < 8 || h > 16
    return LED_MED_INTEN
  end
  return LED_HIGH_INTEN
end
