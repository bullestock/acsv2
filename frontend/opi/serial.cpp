#include "serial.h"
#include "logger.h"
#include "util.h"

#include <thread>

#include <fmt/chrono.h>
#include <fmt/core.h>

Ports detect_ports()
{
    Ports ports;
    for (int port_num = 0; port_num < 3; ++port_num)
    {
        std::string port = fmt::format("/dev/ttyUSB{}", port_num);
        serialib serial;
        const int res = serial.openDevice(port.c_str(), 115200);
        if (res)
        {
            Logger::instance().log(fmt::format("Failed to open {}: {}", port, res));
            continue;
        }

        Logger::instance().log(fmt::format("Opened {}", port));
        serial.clearDTR();
        serial.setRTS();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        serial.clearRTS();
        serial.flushReceiver();

        // Skip prologue
        const int MAX_LEN = 80;
        std::string line;
        for (int i = 0; i < 100; ++i)
        {
            const bool last_empty = line.empty();
            if (!serial.readString(line, '\n', MAX_LEN, 100) && last_empty)
            {
                Logger::instance().log_verbose(fmt::format("{}: Skipped {} lines", port, i));
                break;
            }
            Logger::instance().log_verbose(fmt::format("{}: Skipped: {}", util::now(), util::strip(line)));
            
        }
        int attempts = 0;
        while (++attempts < 100)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            Logger::instance().log_verbose("Send V");
            if (!serial.write("V\n"))
            {
                Logger::instance().log("Write failed");
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            int n = 0;
            do
            {
                const int nof_bytes = serial.readString(line, '\n', MAX_LEN, 100);
                if (nof_bytes > 0)
                    Logger::instance().log_verbose(fmt::format("Got {}", line));
                else if (++n > 10)
                {
                    Logger::instance().log("Timeout");
                    break;
                }
            } while (line.empty());
            line = util::strip(line);
            auto reply = util::strip_np(line);
            Logger::instance().log_verbose(fmt::format("Got {} -> {}", line, reply));
            if (reply == "V")
            {
                // Echo is on
                const int nof_bytes = serial.readString(line, '\n', MAX_LEN, 100);
                line = util::strip(line);
                reply = util::strip_np(line);
                Logger::instance().log_verbose(fmt::format("Got {} -> {}", line, reply));
            }
            if (reply.find("ACS") != std::string::npos)
            {
                Logger::instance().log_verbose(fmt::format("Version: {}", reply));
                if (reply.find("display") != std::string::npos)
                {
                    ports.display = std::move(serial);
                    break;
                }
                else if (reply.find("cardreader") != std::string::npos)
                {
                    ports.reader = std::move(serial);
                    break;
                }
            }
            else if (reply.find("Danalock") != std::string::npos)
            {
                ports.lock = std::move(serial);
                break;
            }
        }
    }

    return std::move(ports);
}
