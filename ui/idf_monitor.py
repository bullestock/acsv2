from __future__ import division, print_function, unicode_literals

import argparse
import codecs
import datetime
import os
import re
import subprocess
import time
from builtins import bytes, chr, object

try:
    import queue
except ImportError:
    import Queue as queue

import ctypes
import json
import shlex
import sys
import tempfile
import textwrap
import threading
import time
import types
from distutils.version import StrictVersion
from io import open

import serial
import serial.tools.list_ports
import serial.tools.miniterm as miniterm

key_description = miniterm.key_description

# Control-key characters
CTRL_A = '\x01'
CTRL_B = '\x02'
CTRL_F = '\x06'
CTRL_H = '\x08'
CTRL_R = '\x12'
CTRL_T = '\x14'
CTRL_Y = '\x19'
CTRL_P = '\x10'
CTRL_X = '\x18'
CTRL_L = '\x0c'
CTRL_RBRACKET = '\x1d'  # Ctrl+]

# Command parsed from console inputs
CMD_STOP = 1
CMD_RESET = 2
CMD_MAKE = 3
CMD_APP_FLASH = 4
CMD_OUTPUT_TOGGLE = 5
CMD_TOGGLE_LOGGING = 6
CMD_ENTER_BOOT = 7

# ANSI terminal codes (if changed, regular expressions in LineMatcher need to be udpated)
ANSI_RED = '\033[1;31m'
ANSI_YELLOW = '\033[0;33m'
ANSI_NORMAL = '\033[0m'


def color_print(message, color, newline='\n'):
    """ Print a message to stderr with colored highlighting """
    sys.stderr.write('%s%s%s%s' % (color, message,  ANSI_NORMAL, newline))


def yellow_print(message, newline='\n'):
    color_print(message, ANSI_YELLOW, newline)


def red_print(message, newline='\n'):
    color_print(message, ANSI_RED, newline)


__version__ = '1.1'

# Tags for tuples in queues
TAG_KEY = 0
TAG_SERIAL = 1
TAG_SERIAL_FLUSH = 2
TAG_CMD = 3

# regex matches an potential PC value (0x4xxxxxxx)
MATCH_PCADDR = re.compile(r'0x4[0-9a-f]{7}', re.IGNORECASE)

DEFAULT_TOOLCHAIN_PREFIX = 'xtensa-esp32-elf-'

DEFAULT_PRINT_FILTER = ''

class StoppableThread(object):
    """
    Provide a Thread-like class which can be 'cancelled' via a subclass-provided
    cancellation method.

    Can be started and stopped multiple times.

    Isn't an instance of type Thread because Python Thread objects can only be run once
    """
    def __init__(self):
        self._thread = None

    @property
    def alive(self):
        """
        Is 'alive' whenever the internal thread object exists
        """
        return self._thread is not None

    def start(self):
        if self._thread is None:
            self._thread = threading.Thread(target=self._run_outer)
            self._thread.start()

    def _cancel(self):
        pass  # override to provide cancellation functionality

    def run(self):
        pass  # override for the main thread behaviour

    def _run_outer(self):
        try:
            self.run()
        finally:
            self._thread = None

    def stop(self):
        if self._thread is not None:
            old_thread = self._thread
            self._thread = None
            self._cancel()
            old_thread.join()

class Killer(threading.Thread):
    def __init__(self, q):
        threading.Thread.__init__(self)
        self.q = q

    def run(self):
        time.sleep(5)
        # Seppuku!
        self.q.put((TAG_CMD, CMD_STOP))

class ConsoleReader(StoppableThread):
    """ Read input keys from the console and push them to the queue,
    until stopped.
    """
    def __init__(self, console, event_queue, cmd_queue, parser):
        super(ConsoleReader, self).__init__()
        self.console = console
        self.event_queue = event_queue
        self.cmd_queue = cmd_queue
        self.parser = parser

    def run(self):
        self.console.setup()
        try:
            while self.alive:
                try:
                    c = self.console.getkey()
                except KeyboardInterrupt:
                    c = '\x03'
                if c is not None:
                    ret = self.parser.parse(c)
                    if ret is not None:
                        (tag, cmd) = ret
                        # stop command should be executed last
                        if tag == TAG_CMD and cmd != CMD_STOP:
                            self.cmd_queue.put(ret)
                        else:
                            self.event_queue.put(ret)

        finally:
            self.console.cleanup()

    def _cancel(self):
        import fcntl
        import termios
        fcntl.ioctl(self.console.fd, termios.TIOCSTI, b'\0')


class ConsoleParser(object):

    def __init__(self, eol='CRLF'):
        self.translate_eol = {
            'CRLF': lambda c: c.replace('\n', '\r\n'),
            'CR': lambda c: c.replace('\n', '\r'),
            'LF': lambda c: c.replace('\r', '\n'),
        }[eol]
        self.menu_key = CTRL_T
        self.exit_key = CTRL_RBRACKET
        self._pressed_menu_key = False

    def parse(self, key):
        ret = None
        if self._pressed_menu_key:
            ret = self._handle_menu_key(key)
        elif key == self.menu_key:
            self._pressed_menu_key = True
        elif key == self.exit_key:
            ret = (TAG_CMD, CMD_STOP)
        else:
            key = self.translate_eol(key)
            ret = (TAG_KEY, key)
        return ret

    def _handle_menu_key(self, c):
        ret = None
        if c == self.exit_key or c == self.menu_key:  # send verbatim
            ret = (TAG_KEY, c)
        elif c in [CTRL_H, 'h', 'H', '?']:
            red_print(self.get_help_text())
        elif c == CTRL_R:  # Reset device via RTS
            ret = (TAG_CMD, CMD_RESET)
        elif c == CTRL_F:  # Recompile & upload
            ret = (TAG_CMD, CMD_MAKE)
        elif c in [CTRL_A, 'a', 'A']:  # Recompile & upload app only
            # "CTRL-A" cannot be captured with the default settings of the Windows command line, therefore, "A" can be used
            # instead
            ret = (TAG_CMD, CMD_APP_FLASH)
        elif c == CTRL_Y:  # Toggle output display
            ret = (TAG_CMD, CMD_OUTPUT_TOGGLE)
        elif c == CTRL_L:  # Toggle saving output into file
            ret = (TAG_CMD, CMD_TOGGLE_LOGGING)
        elif c == CTRL_P:
            yellow_print('Pause app (enter bootloader mode), press Ctrl-T Ctrl-R to restart')
            # to fast trigger pause without press menu key
            ret = (TAG_CMD, CMD_ENTER_BOOT)
        elif c in [CTRL_X, 'x', 'X']:  # Exiting from within the menu
            ret = (TAG_CMD, CMD_STOP)
        else:
            red_print('--- unknown menu character {} --'.format(key_description(c)))

        self._pressed_menu_key = False
        return ret

    def get_help_text(self):
        text = """\
            --- idf_monitor ({version}) - ESP-IDF monitor tool
            --- based on miniterm from pySerial
            ---
            --- {exit:8} Exit program
            --- {menu:8} Menu escape key, followed by:
            --- Menu keys:
            ---    {menu:14} Send the menu character itself to remote
            ---    {exit:14} Send the exit character itself to remote
            ---    {reset:14} Reset target board via RTS line
            ---    {makecmd:14} Build & flash project
            ---    {appmake:14} Build & flash app only
            ---    {output:14} Toggle output display
            ---    {log:14} Toggle saving output into file
            ---    {pause:14} Reset target into bootloader to pause app via RTS line
            ---    {menuexit:14} Exit program
        """.format(version=__version__,
                   exit=key_description(self.exit_key),
                   menu=key_description(self.menu_key),
                   reset=key_description(CTRL_R),
                   makecmd=key_description(CTRL_F),
                   appmake=key_description(CTRL_A) + ' (or A)',
                   output=key_description(CTRL_Y),
                   log=key_description(CTRL_L),
                   pause=key_description(CTRL_P),
                   menuexit=key_description(CTRL_X) + ' (or X)')
        return textwrap.dedent(text)

    def get_next_action_text(self):
        text = """\
            --- Press {} to exit monitor.
            --- Press {} to build & flash project.
            --- Press {} to build & flash app.
            --- Press any other key to resume monitor (resets target).
        """.format(key_description(self.exit_key),
                   key_description(CTRL_F),
                   key_description(CTRL_A))
        return textwrap.dedent(text)

    def parse_next_action_key(self, c):
        ret = None
        if c == self.exit_key:
            ret = (TAG_CMD, CMD_STOP)
        elif c == CTRL_F:  # Recompile & upload
            ret = (TAG_CMD, CMD_MAKE)
        elif c in [CTRL_A, 'a', 'A']:  # Recompile & upload app only
            # "CTRL-A" cannot be captured with the default settings of the Windows command line, therefore, "A" can be used
            # instead
            ret = (TAG_CMD, CMD_APP_FLASH)
        return ret


class SerialReader(StoppableThread):
    """ Read serial data from the serial port and push to the
    event queue, until stopped.
    """
    def __init__(self, serial, event_queue):
        super(SerialReader, self).__init__()
        self.baud = serial.baudrate
        self.serial = serial
        self.event_queue = event_queue
        if not hasattr(self.serial, 'cancel_read'):
            # enable timeout for checking alive flag,
            # if cancel_read not available
            self.serial.timeout = 0.25

    def run(self):
        if not self.serial.is_open:
            self.serial.baudrate = self.baud
            self.serial.rts = True  # Force an RTS reset on open
            self.serial.open()
            time.sleep(0.005)  # Add a delay to meet the requirements of minimal EN low time (2ms for ESP32-C3)
            self.serial.rts = False
            self.serial.dtr = self.serial.dtr   # usbser.sys workaround
        try:
            while self.alive:
                try:
                    data = self.serial.read(self.serial.in_waiting or 1)
                except (serial.serialutil.SerialException, IOError) as e:
                    data = b''
                    # self.serial.open() was successful before, therefore, this is an issue related to
                    # the disappearance of the device
                    red_print(e)
                    yellow_print('Waiting for the device to reconnect', newline='')
                    self.serial.close()
                    while self.alive:  # so that exiting monitor works while waiting
                        try:
                            time.sleep(0.5)
                            self.serial.open()
                            break  # device connected
                        except serial.serialutil.SerialException:
                            yellow_print('.', newline='')
                            sys.stderr.flush()
                    yellow_print('')  # go to new line
                if len(data):
                    self.event_queue.put((TAG_SERIAL, data), False)
        finally:
            self.serial.close()

    def _cancel(self):
        if hasattr(self.serial, 'cancel_read'):
            try:
                self.serial.cancel_read()
            except Exception:
                pass


class LineMatcher(object):
    """
    Assembles a dictionary of filtering rules based on the --print_filter
    argument of idf_monitor. Then later it is used to match lines and
    determine whether they should be shown on screen or not.
    """
    LEVEL_N = 0
    LEVEL_E = 1
    LEVEL_W = 2
    LEVEL_I = 3
    LEVEL_D = 4
    LEVEL_V = 5

    level = {'N': LEVEL_N, 'E': LEVEL_E, 'W': LEVEL_W, 'I': LEVEL_I, 'D': LEVEL_D,
             'V': LEVEL_V, '*': LEVEL_V, '': LEVEL_V}

    def __init__(self, print_filter):
        self._dict = dict()
        self._re = re.compile(r'^(?:\033\[[01];?[0-9]+m?)?([EWIDV]) \([0-9]+\) ([^:]+): ')
        items = print_filter.split()
        if len(items) == 0:
            self._dict['*'] = self.LEVEL_V  # default is to print everything
        for f in items:
            s = f.split(r':')
            if len(s) == 1:
                # specifying no warning level defaults to verbose level
                lev = self.LEVEL_V
            elif len(s) == 2:
                if len(s[0]) == 0:
                    raise ValueError('No tag specified in filter ' + f)
                try:
                    lev = self.level[s[1].upper()]
                except KeyError:
                    raise ValueError('Unknown warning level in filter ' + f)
            else:
                raise ValueError('Missing ":" in filter ' + f)
            self._dict[s[0]] = lev

    def match(self, line):
        try:
            m = self._re.search(line)
            if m:
                lev = self.level[m.group(1)]
                if m.group(2) in self._dict:
                    return self._dict[m.group(2)] >= lev
                return self._dict.get('*', self.LEVEL_N) >= lev
        except (KeyError, IndexError):
            # Regular line written with something else than ESP_LOG*
            # or an empty line.
            pass
        # We need something more than "*.N" for printing.
        return self._dict.get('*', self.LEVEL_N) > self.LEVEL_N


class SerialStopException(Exception):
    """
    This exception is used for stopping the IDF monitor in testing mode.
    """
    pass


class Monitor(object):
    """
    Monitor application main class.

    This was originally derived from miniterm.Miniterm, but it turned out to be easier to write from scratch for this
    purpose.

    Main difference is that all event processing happens in the main thread, not the worker threads.
    """
    def __init__(self, serial_instance):
        super(Monitor, self).__init__()
        self.event_queue = queue.Queue()
        self.cmd_queue = queue.Queue()
        self.console = miniterm.Console()
        self.enable_address_decoding = False

        if StrictVersion(serial.VERSION) < StrictVersion('3.3.0'):
            # Use Console.getkey implementation from 3.3.0 (to be in sync with the ConsoleReader._cancel patch above)
            def getkey_patched(self):
                c = self.enc_stdin.read(1)
                if c == chr(0x7f):
                    c = chr(8)    # map the BS key (which yields DEL) to backspace
                return c

            self.console.getkey = types.MethodType(getkey_patched, self.console)

        self.serial = serial_instance
        self.console_parser = ConsoleParser('CRLF')
        self.console_reader = ConsoleReader(self.console, self.event_queue, self.cmd_queue, self.console_parser)
        self.serial_reader = SerialReader(self.serial, self.event_queue)
        self.killer = Killer(self.event_queue)
        self.elf_file = None
        self.make = ''
        self.encrypted = ''
        self.toolchain_prefix = ''
        self.websocket_client = None
        #self.target = target

        # internal state
        self._last_line_part = b''
        self._line_matcher = LineMatcher('')
        self._invoke_processing_last_line_timer = None
        self._force_line_print = False
        self._output_enabled = True
        self._log_file = None

    def invoke_processing_last_line(self):
        self.event_queue.put((TAG_SERIAL_FLUSH, b''), False)

    def main_loop(self):
        self.console_reader.start()
        self.serial_reader.start()
        self.killer.start()
        started = time.time()
        try:
            while self.console_reader.alive and self.serial_reader.alive:
                try:
                    item = self.cmd_queue.get_nowait()
                    if time.time() - started > 10:
                        print("Hammerzeit!")
                        self.console_reader.stop()
                        self.serial_reader.stop()
                except queue.Empty:
                    try:
                        item = self.event_queue.get(True, 0.03)
                    except queue.Empty:
                        continue
                (event_tag, data) = item
                if event_tag == TAG_CMD:
                    self.handle_commands(data)
                elif event_tag == TAG_KEY:
                    try:
                        self.serial.write(codecs.encode(data))
                    except serial.SerialException:
                        pass  # this shouldn't happen, but sometimes port has closed in serial thread
                    except UnicodeEncodeError:
                        pass  # this can happen if a non-ascii character was passed, ignoring
                elif event_tag == TAG_SERIAL:
                    self.handle_serial_input(data)
                    if self._invoke_processing_last_line_timer is not None:
                        self._invoke_processing_last_line_timer.cancel()
                    self._invoke_processing_last_line_timer = threading.Timer(0.1, self.invoke_processing_last_line)
                    self._invoke_processing_last_line_timer.start()
                    # If no further data is received in the next short period
                    # of time then the _invoke_processing_last_line_timer
                    # generates an event which will result in the finishing of
                    # the last line. This is fix for handling lines sent
                    # without EOL.
                elif event_tag == TAG_SERIAL_FLUSH:
                    self.handle_serial_input(data, finalize_line=True)
                else:
                    raise RuntimeError('Bad event data %r' % ((event_tag,data),))
        except SerialStopException:
            sys.stderr.write(ANSI_NORMAL + 'Stopping condition has been received\n')
        finally:
            try:
                self.console_reader.stop()
                self.serial_reader.stop()
                self.stop_logging()
                # Cancelling _invoke_processing_last_line_timer is not
                # important here because receiving empty data doesn't matter.
                self._invoke_processing_last_line_timer = None
            except Exception:
                pass
            sys.stderr.write(ANSI_NORMAL + '\n')

    def handle_serial_input(self, data, finalize_line=False):
        sp = data.split(b'\n')
        if self._last_line_part != b'':
            # add unprocessed part from previous "data" to the first line
            sp[0] = self._last_line_part + sp[0]
            self._last_line_part = b''
        if sp[-1] != b'':
            # last part is not a full line
            self._last_line_part = sp.pop()
        for line in sp:
            if line != b'':
                if line == self.console_parser.exit_key.encode('latin-1'):
                    raise SerialStopException()
                if self._force_line_print or self._line_matcher.match(line.decode(errors='ignore')):
                    self._print(line + b'\n')
                self._force_line_print = False
        # Now we have the last part (incomplete line) in _last_line_part. By
        # default we don't touch it and just wait for the arrival of the rest
        # of the line. But after some time when we didn't received it we need
        # to make a decision.
        if self._last_line_part != b'':
            if self._force_line_print or (finalize_line and self._line_matcher.match(self._last_line_part.decode(errors='ignore'))):
                self._force_line_print = True
                self._print(self._last_line_part)
                self._last_line_part = b''
        # else: keeping _last_line_part and it will be processed the next time
        # handle_serial_input is invoked

    def __enter__(self):
        """ Use 'with self' to temporarily disable monitoring behaviour """
        self.serial_reader.stop()
        self.console_reader.stop()

    def __exit__(self, *args, **kwargs):
        """ Use 'with self' to temporarily disable monitoring behaviour """
        self.console_reader.start()
        self.serial_reader.start()

    def prompt_next_action(self, reason):
        self.console.setup()  # set up console to trap input characters
        try:
            red_print('--- {}'.format(reason))
            red_print(self.console_parser.get_next_action_text())

            k = CTRL_T  # ignore CTRL-T here, so people can muscle-memory Ctrl-T Ctrl-F, etc.
            while k == CTRL_T:
                k = self.console.getkey()
        finally:
            self.console.cleanup()
        ret = self.console_parser.parse_next_action_key(k)
        if ret is not None:
            cmd = ret[1]
            if cmd == CMD_STOP:
                # the stop command should be handled last
                self.event_queue.put(ret)
            else:
                self.cmd_queue.put(ret)

    def output_enable(self, enable):
        self._output_enabled = enable

    def output_toggle(self):
        self._output_enabled = not self._output_enabled
        yellow_print('\nToggle output display: {}, Type Ctrl-T Ctrl-Y to show/disable output again.'.format(self._output_enabled))

    def toggle_logging(self):
        if self._log_file:
            self.stop_logging()
        else:
            self.start_logging()

    def start_logging(self):
        if not self._log_file:
            try:
                name = 'log.{}.{}.txt'.format(os.path.splitext(os.path.basename(self.elf_file))[0],
                                              datetime.datetime.now().strftime('%Y%m%d%H%M%S'))
                self._log_file = open(name, 'wb+')
                yellow_print('\nLogging is enabled into file {}'.format(name))
            except Exception as e:
                red_print('\nLog file {} cannot be created: {}'.format(name, e))

    def stop_logging(self):
        if self._log_file:
            try:
                name = self._log_file.name
                self._log_file.close()
                yellow_print('\nLogging is disabled and file {} has been closed'.format(name))
            except Exception as e:
                red_print('\nLog file cannot be closed: {}'.format(e))
            finally:
                self._log_file = None

    def _print(self, string, console_printer=None):
        if console_printer is None:
            console_printer = self.console.write_bytes
        if self._output_enabled:
            console_printer(string)
        if self._log_file:
            try:
                if isinstance(string, type(u'')):
                    string = string.encode()
                self._log_file.write(string)
            except Exception as e:
                red_print('\nCannot write to file: {}'.format(e))
                # don't fill-up the screen with the previous errors (probably consequent prints would fail also)
                self.stop_logging()

    def handle_commands(self, cmd):
        if cmd == CMD_STOP:
            self.console_reader.stop()
            self.serial_reader.stop()
        elif cmd == CMD_RESET:
            self.serial.setRTS(True)
            self.serial.setDTR(self.serial.dtr)  # usbser.sys workaround
            time.sleep(0.2)
            self.serial.setRTS(False)
            self.serial.setDTR(self.serial.dtr)  # usbser.sys workaround
            self.output_enable(True)
        elif cmd == CMD_OUTPUT_TOGGLE:
            self.output_toggle()
        elif cmd == CMD_ENTER_BOOT:
            self.serial.setDTR(False)  # IO0=HIGH
            self.serial.setRTS(True)   # EN=LOW, chip in reset
            self.serial.setDTR(self.serial.dtr)  # usbser.sys workaround
            time.sleep(1.3)  # timeouts taken from esptool.py, includes esp32r0 workaround. defaults: 0.1
            self.serial.setDTR(True)   # IO0=LOW
            self.serial.setRTS(False)  # EN=HIGH, chip out of reset
            self.serial.setDTR(self.serial.dtr)  # usbser.sys workaround
            time.sleep(0.45)  # timeouts taken from esptool.py, includes esp32r0 workaround. defaults: 0.05
            self.serial.setDTR(False)  # IO0=HIGH, done
        else:
            raise RuntimeError('Bad command data %d' % (cmd))


def main():
    parser = argparse.ArgumentParser('idf_monitor - a serial output monitor for esp-idf')

    parser.add_argument(
        '--port', '-p',
        help='Serial port device',
        default=os.environ.get('ESPTOOL_PORT', '/dev/ttyUSB0')
    )

    parser.add_argument(
        '--baud', '-b',
        help='Serial port baud rate',
        type=int,
        default=os.getenv('IDF_MONITOR_BAUD', os.getenv('MONITORBAUD', 115200)))

    args = parser.parse_args()

    serial_instance = serial.Serial()
    serial_instance.port = args.port
    serial_instance.baudrate = args.baud
    serial_instance.dtr = False
    serial_instance.rts = False

    monitor = Monitor(serial_instance)

    monitor.main_loop()

if __name__ == '__main__':
    main()
