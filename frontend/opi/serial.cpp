#include "serial.h"
#include "util.h"

#include <thread>

#include <fmt/core.h>

Ports detect_ports()
{
    serialib s1, s2;
    s1 = std::move(s2);
    
    Ports ports;
    for (int port_num = 0; port_num < 3; ++port_num)
    {
        std::string port = fmt::format("/dev/ttyUSB{}", port_num);
        serialib serial;
        const int res = serial.openDevice(port.c_str(), 115200);
        if (res)
        {
            std::cout << "Failed to open " << port << ": " << res << std::endl;
        }
        else
        {
            std::cout << "Opened " << port << std::endl;
            
            const int MAX_LEN = 80;
            while (1)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                std::cout << "Send V" << std::endl;
                if (!serial.write("V\n"))
                    std::cout << "Write failed\n";
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                std::string line;
                do
                {
                    const int nof_bytes = serial.readString(line, '\n', MAX_LEN, 100);
                    if (nof_bytes > 0)
                        std::cout << "Got " << line << std::endl;
                } while (line.empty());
                line = util::strip(line);
                auto reply = util::strip_ascii(line);
                std::cout << "Got " << line << " -> " << reply << std::endl;
                if (reply == "V")
                {
                    // Echo is on
                    const int nof_bytes = serial.readString(line, '\n', MAX_LEN, 100);
                    line = util::strip(line);
                    reply = util::strip_ascii(line);
                    std::cout << "Got " << line << " -> " << reply << std::endl;
                }
                if (reply.find("ACS") != std::string::npos)
                {
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
    }

    return std::move(ports);
}
