#!/usr/bin/ruby

require 'serialport'

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
      puts "Opened #{port}"
    end
  end
end
