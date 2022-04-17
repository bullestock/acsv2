#include "display.h"

#include <fmt/core.h>

Display::Display(serialib& p)
    : port(p),
      thread([this](){ thread_body(); }),
      q(10)
{
}

Display::~Display()
{
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
}

void Display::show_message(const std::string& text,
                           Color col,
                           util::duration dur)
{
    Item item{ Item::Type::Show_message, col, "", 0, dur };
    strncpy(item.s, text.c_str(), std::min<size_t>(Item::MAX_SIZE, text.size()));
    q.push(item);
}

void Display::show_info(int line,
                        const std::string& text,
                        Color col)
{
    Item item{ Item::Type::Show_info, col, "", line };
    strncpy(item.s, text.c_str(), std::min<size_t>(Item::MAX_SIZE, text.size()));
    q.push(item);
}

constexpr auto MAX_LINE_LEN = 20; //!!

void Display::thread_body()
{
    Item item;
    Color last_status_color = Color::white;
    std::string last_status;
    while (1)
    {
        if (!q.empty() && q.pop(item))
            switch (item.type)
            {
            case Item::Type::Clear:
                port.write("C\n");
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
                std::cout << "Item type " << int(item.type) << " not handled\n";
                break;
            }
        else
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if ((clear_at != util::time_point()) &&
            (util::now() >= clear_at))
        {
            clear_at = util::time_point();
            do_show_message(last_status, last_status_color);
        }
    }
}

void Display::do_show_message(const std::string& msg, Color color)
{
    if (msg.size() > MAX_LINE_LEN)
    {
        const auto [line1, line2] = util::wrap(msg);
        port.write(fmt::format("TE,2,{},{}\n", static_cast<int>(color), line1));
        port.write(fmt::format("TE,3,{},{}\n", static_cast<int>(color), line2));
        return;
    }
    port.write(fmt::format("TE,2,{},{}\n", static_cast<int>(color), msg));
}

void Display::do_show_info(int line,
                           const std::string& text,
                           Color color)
{
    port.write(fmt::format("te,{},{},{}\n", line, static_cast<int>(color), text));
}
