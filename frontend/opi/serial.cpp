#include "serial.h"
#include "util.h"

#include <thread>

#include <fmt/chrono.h>
#include <fmt/core.h>

Ports detect_ports(bool verbose)
{
    Ports ports;
    for (int port_num = 0; port_num < 3; ++port_num)
    {
        std::string port = fmt::format("/dev/ttyUSB{}", port_num);
        serialib serial;
        const int res = serial.openDevice(port.c_str(), 115200);
        if (res)
        {
            std::cout << "Failed to open " << port << ": " << res << std::endl;
            continue;
        }

        std::cout << "Opened " << port << std::endl;
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
                if (verbose)
                    std::cout << fmt::format("{}: Skipped {} lines", port, i) << std::endl;
                break;
            }
            if (verbose)
                std::cout << fmt::format("{}: Skipped: {}", util::now(), util::strip(line)) << std::endl;
            
        }
        int attempts = 0;
        while (++attempts < 100)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            if (verbose)
                std::cout << "Send V" << std::endl;
            if (!serial.write("V\n"))
            {
                std::cout << "Write failed\n";
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            int n = 0;
            do
            {
                const int nof_bytes = serial.readString(line, '\n', MAX_LEN, 100);
                if (verbose && nof_bytes > 0)
                    std::cout << "Got " << line << std::endl;
                else if (++n > 10)
                {
                    std::cout << "Timeout\n";
                    break;
                }
            } while (line.empty());
            line = util::strip(line);
            auto reply = util::strip_np(line);
            if (verbose)
                std::cout << "Got " << line << " -> " << reply << std::endl;
            if (reply == "V")
            {
                // Echo is on
                const int nof_bytes = serial.readString(line, '\n', MAX_LEN, 100);
                line = util::strip(line);
                reply = util::strip_np(line);
                if (verbose)
                    std::cout << "Got " << line << " -> " << reply << std::endl;
            }
            if (reply.find("ACS") != std::string::npos)
            {
                if (verbose)
                    std::cout << "Version: " << reply << "\n";
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
