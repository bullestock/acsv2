#include "display.h"
#include "logger.h"

#include <fmt/core.h>
#include <fmt/chrono.h>

Display::Display(serialib& p)
    : port(p),
      thread([this](){ thread_body(); }),
      q(10)
{
    for (int i = 0; i < NOF_LINES; ++i)
    {
        cur_buffer.push_back(std::make_pair("", Color::white));
        new_buffer.push_back(std::make_pair("", Color::white));
    }
}

Display::~Display()
{
    stop = true;
    if (thread.joinable())
        thread.join();
}

void Display::clear()
{
    q.push({ Item::Type::Clear });
}

void Display::set_status(const std::string& text,
                         Color col)
{
    Item item{ Item::Type::Set_status, col };
    strncpy(item.s, text.c_str(), std::min<size_t>(Item::MAX_SIZE, text.size()));
    q.push(item);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

void Display::show_message(const std::string& text,
                           Color col,
                           util::duration dur)
{
    Item item{ Item::Type::Show_message, col, "", 0, dur };
    strncpy(item.s, text.c_str(), std::min<size_t>(Item::MAX_SIZE, text.size()));
    q.push(item);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

void Display::show_info(int line,
                        const std::string& text,
                        Color col)
{
    Item item{ Item::Type::Show_info, col, "", line };
    strncpy(item.s, text.c_str(), std::min<size_t>(Item::MAX_SIZE, text.size()));
    q.push(item);
}

std::string Display::get_reply(const std::string& cmd)
{
    std::string line;
    while (1)
    {
        port.readString(line, '\n', 50, 100);
        if (line.substr(0, 5) == std::string("DEBUG"))
            continue;
        line = util::strip(line);
        if (line.empty() || line == cmd)
            // echo
            continue;
        if (line != "OK")
            Logger::instance().log(fmt::format("ERROR: Display replied '{}'", line));
        return line;
    }
}

constexpr auto MAX_LINE_LEN = 20; //!!

void Display::thread_body()
{
    port.flushReceiver();
    Item item;
    Color last_status_color = Color::white;
    std::string last_status;
    auto last_clock = util::now();
    while (!stop)
    {
        if (!q.empty() && q.pop(item))
            switch (item.type)
            {
            case Item::Type::Clear:
                port.write("C\n");
                get_reply("C");
                break;

            case Item::Type::Set_status:
                if (item.s != last_status || item.color != last_status_color)
                {
                    do_show_message(item.s, item.color);
                    last_status = item.s;
                    last_status_color = item.color;
                }
                break;
               
            case Item::Type::Show_message:
                do_show_message(item.s, item.color);
                clear_at = util::now() + item.dur;
                break;

            case Item::Type::Show_info:
                do_show_info(item.line_no, item.s, item.color);
                clear_at = util::now() + item.dur;
                break;

            default:
                Logger::instance().log(fmt::format("Item type {} not handled", int(item.type)));
                break;
            }
        else
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if ((clear_at != util::time_point()) &&
            (util::now() >= clear_at))
        {
            clear_at = util::time_point();
            do_show_message(last_status, last_status_color);
        }
        const auto clock = std::chrono::floor<std::chrono::seconds>(util::now());
        if (clock != last_clock)
        {
            last_clock = clock;
            do_show_info(12, fmt::format("{:%H:%M:%S}", fmt::localtime(clock)), Color::white);
        }
    }
}

void Display::do_show_message(const std::string& msg, Color color)
{
    for (auto& line : new_buffer)
        line.first.clear();
    if (msg.size() > MAX_LINE_LEN)
    {
        const auto lines = util::wrap(msg, MAX_LINE_LEN);
        const int center_line = 3;
        int line = center_line - lines.size()/2;
        int index = 0;
        while (index < lines.size())
        {
            new_buffer[line+index] = std::make_pair(lines[index], color);
            ++index;
        }
    }
    else
        new_buffer[NOF_LINES/2] = std::make_pair(msg, color);
    sync();
}

void Display::sync()
{
    for (int i = 0; i < NOF_LINES; ++i)
    {
        if (cur_buffer[i] == new_buffer[i])
            continue;
        const auto cmd = fmt::format("TE,{},{},{}\n", i, static_cast<int>(new_buffer[i].second), new_buffer[i].first);
        port.write(cmd + "\n");
        get_reply(cmd);
        cur_buffer[i] = new_buffer[i];
    }
}

void Display::do_show_info(int line,
                           const std::string& text,
                           Color color)
{
    port.write(fmt::format("te,{},{},{}\n", line, static_cast<int>(color), text));
}
