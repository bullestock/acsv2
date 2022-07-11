#include "serial.h"
#include "logger.h"
#include "util.h"

#include <thread>

#include <fmt/chrono.h>
#include <fmt/core.h>

static constexpr int MAX_LEN = 80;

void skip_prologue(serialib& serial, const std::string& port)
{
    std::string line;
    for (int i = 0; i < 120; ++i)
    {
        const bool last_empty = line.empty();
        if (!serial.readString(line, '\n', MAX_LEN, 500) && last_empty)
        {
            Logger::instance().log_verbose(fmt::format("{}: Skipped {} lines", port, i));
            break;
        }
        Logger::instance().log_verbose(fmt::format("{}: Skipped: {}", util::now(), util::strip(line)));
    }
}

void detect_port(int port_num, Ports& ports)
{
    std::string port = fmt::format("/dev/ttyUSB{}", port_num);
    serialib serial;
    const int res = serial.openDevice(port.c_str(), 115200);
    if (res)
    {
        Logger::instance().log(fmt::format("Failed to open {}: {}", port, res));
        return;
    }

    Logger::instance().log(fmt::format("Opened {}", port));

    // First try without reset
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    serial.clearRTS();
    serial.flushReceiver();
    Logger::instance().log_verbose("Send V");
    if (!serial.write("V\n"))
    {
        Logger::instance().log("Write failed");
        return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    int n = 0;
    std::string line;
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
    if (line.empty())
    {
        // Reset
        Logger::instance().log("Resetting");
        serial.clearDTR();
        serial.setRTS();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        serial.clearRTS();
        serial.flushReceiver();

        // Skip prologue
        skip_prologue(serial, port);
        int attempts = 0;
        line.clear();
        while (++attempts < 100)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            Logger::instance().log_verbose("Send V");
            if (!serial.write("V\n"))
            {
                Logger::instance().log("Write failed");
                return;
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
            if (line.find("ACS") != std::string::npos)
                break;                
        }
    }
    line = util::strip(line);
    auto reply = util::strip_np(line);
    Logger::instance().log_verbose(fmt::format("Got {} -> {}", line, reply));
    if (reply == "V")
    {
        // Echo is on
        const int nof_bytes = serial.readString(line, '\n', MAX_LEN, 100);
        line = util::strip(line);
        reply = util::strip_np(line);
        Logger::instance().log_verbose(fmt::format("Echo on: Got {} -> {}", line, reply));
    }
    if (reply.find("ACS") != std::string::npos)
    {
        Logger::instance().log_verbose(fmt::format("Version: {}", reply));
        if (reply.find("display") != std::string::npos)
        {
            Logger::instance().log_verbose(fmt::format("Display is {}", port));
            ports.display = std::move(serial);
            return;
        }
        if (reply.find("cardreader") != std::string::npos)
        {
            Logger::instance().log_verbose(fmt::format("Card reader is {}", port));
            ports.reader = std::move(serial);
            return;
        }
    }
    if (reply.find("Danalock") != std::string::npos)
    {
        Logger::instance().log_verbose(fmt::format("Lock is {}", port));
        ports.lock = std::move(serial);
    }
}

Ports detect_ports()
{
    Ports ports;
    for (int port_num = 0; port_num < 6; ++port_num)
    {
        detect_port(port_num, ports);
        if (ports.complete())
            break;
    }

    return std::move(ports);
}

bool Ports::complete() const
{
    return display.is_open() &&
       lock.is_open() &&
       reader.is_open();
}
