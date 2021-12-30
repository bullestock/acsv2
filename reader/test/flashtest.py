import serial
import string
import struct
import sys
import time

def hexify(s, uppercase=True):
    format_str = '%02X' if uppercase else '%02x'
    return ''.join(format_str % c for c in s)

class HexFormatter(object):
    """
    Wrapper class which takes binary data in its constructor
    and returns a hex string as its __str__ method.

    This is intended for "lazy formatting" of trace() output
    in hex format. Avoids overhead (significant on slow computers)
    of generating long hex strings even if tracing is disabled.

    Note that this doesn't save any overhead if passed as an
    argument to "%", only when passed to trace()

    If auto_split is set (default), any long line (> 16 bytes) will be
    printed as separately indented lines, with ASCII decoding at the end
    of each line.
    """
    def __init__(self, binary_string, auto_split=True):
        self._s = binary_string
        self._auto_split = auto_split

    def __str__(self):
        if self._auto_split and len(self._s) > 16:
            result = ""
            s = self._s
            while len(s) > 0:
                line = s[:16]
                ascii_line = "".join(c if (c == ' ' or (c in string.printable and c not in string.whitespace))
                                     else '.' for c in line.decode('ascii', 'replace'))
                s = s[16:]
                result += "\n    %-16s %-16s | %s" % (hexify(line[:8], False), hexify(line[8:], False), ascii_line)
            return result
        else:
            return hexify(self._s, False)

def slip_reader(port):
    """Generator to read SLIP packets from a serial port.
    Yields one full SLIP packet at a time, raises exception on timeout or invalid data.

    Designed to avoid too many calls to serial.read(1), which can bog
    down on slow systems.
    """
    partial_packet = None
    in_escape = False
    while True:
        waiting = port.inWaiting()
        read_bytes = port.read(1 if waiting == 0 else waiting)
        if read_bytes == b'':
            waiting_for = "header" if partial_packet is None else "content"
            #print("Timed out waiting for packet %s\n" % waiting_for)
            raise FatalError("Timed out waiting for packet %s" % waiting_for)
        #print("Read %d bytes: %s\n" % (len(read_bytes), HexFormatter(read_bytes)))
        for b in read_bytes:
            if type(b) is int:
                b = bytes([b])  # python 2/3 compat

            if partial_packet is None:  # waiting for packet header
                if b == b'\xc0':
                    partial_packet = b""
                else:
                    #trace_function("Read invalid data: %s", HexFormatter(read_bytes))
                    #trace_function("Remaining data in serial buffer: %s", HexFormatter(port.read(port.inWaiting())))
                    raise FatalError('Invalid head of packet (0x%s)' % hexify(b))
            elif in_escape:  # part-way through escape sequence
                in_escape = False
                if b == b'\xdc':
                    partial_packet += b'\xc0'
                elif b == b'\xdd':
                    partial_packet += b'\xdb'
                else:
                    #trace_function("Read invalid data: %s", HexFormatter(read_bytes))
                    #trace_function("Remaining data in serial buffer: %s", HexFormatter(port.read(port.inWaiting())))
                    raise FatalError('Invalid SLIP escape (0xdb, 0x%s)' % (hexify(b)))
            elif b == b'\xdb':  # start of escape sequence
                in_escape = True
            elif b == b'\xc0':  # end of packet
                #print("Received full packet: %s\n" % HexFormatter(partial_packet))
                print("+", end='')
                yield partial_packet
                partial_packet = None
            else:  # normal byte in packet
                partial_packet += b

class FatalError(RuntimeError):
    """
    Wrapper class for runtime errors that aren't caused by internal bugs, but by
    ESP8266 responses or input content.
    """
    def __init__(self, message):
        RuntimeError.__init__(self, message)

    @staticmethod
    def WithResult(message, result):
        """
        Return a fatal error object that appends the hex values of
        'result' as a string formatted argument.
        """
        message += " (result was %s)" % hexify(result)
        return FatalError(message)

class Port:
    def __init__(self, serial_port = "/dev/ttyUSB0"):
        self.ser = serial.Serial(port = serial_port,
                                 baudrate = 115200,
                                 timeout = 1.0)
        self.ser.flushInput()
        self._slip_reader = slip_reader(self.ser)

    def sync(self):
        data = b'\x07\x07\x12\x20' + 32 * b'\x55'
        pkt = struct.pack(b'<BBHI', 0x00, 0x08, len(data), 0) + data
        self.write(pkt)
        time.sleep(0.1)
        for retry in range(100):
            p = self.read()
            if len(p) < 8:
                continue
            (resp, op_ret, len_ret, val) = struct.unpack('<BBHI', p[:8])
            if resp != 1:
                continue
            data = p[8:]
            break

    """ Write bytes to the serial port while performing SLIP escaping """
    def write(self, packet):
        buf = b'\xc0' \
              + (packet.replace(b'\xdb', b'\xdb\xdd').replace(b'\xc0', b'\xdb\xdc')) \
              + b'\xc0'
        #print("Write %d bytes: %s\n" % (len(buf), HexFormatter(buf)))
        self.ser.write(buf)

    """ Read a SLIP packet from the serial port """
    def read(self):
        return next(self._slip_reader)

    def flush_input(self):
        self.ser.flushInput()
        self._slip_reader = slip_reader(self.ser)

    def connect(self, esp32r0_delay=False):
        self.ser.setDTR(False)  # IO0=HIGH
        self.ser.setRTS(True)   # EN=LOW, chip in reset
        time.sleep(0.1)
        if esp32r0_delay:
            # Some chips are more likely to trigger the esp32r0
            # watchdog reset silicon bug if they're held with EN=LOW
            # for a longer period
            time.sleep(1.2)
        self.ser.setDTR(True)   # IO0=LOW
        self.ser.setRTS(False)  # EN=HIGH, chip out of reset
        if esp32r0_delay:
            # Sleep longer after reset.
            # This workaround only works on revision 0 ESP32 chips,
            # it exploits a silicon bug spurious watchdog reset.
            time.sleep(0.4)  # allow watchdog reset to occur
        time.sleep(0.05)
        self.ser.setDTR(False)  # IO0=HIGH, done

        for _ in range(5):
            try:
                self.flush_input()
                self.ser.flushOutput()
                self.sync()
                return None
            except FatalError as e:
                if esp32r0_delay:
                    print('_', end='')
                else:
                    print('.', end='')
                sys.stdout.flush()
                time.sleep(0.05)

            
if __name__ == "__main__":
    l = Port()
    while True:
        l.connect(True)
        l.connect(False)

