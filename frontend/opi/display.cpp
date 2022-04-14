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
    Item item{ Item::Type::Show_message, col, "", dur };
    strncpy(item.s, text.c_str(), std::min<size_t>(Item::MAX_SIZE, text.size()));
    q.push(item);
}

void Display::thread_body()
{
    Item item;
    while (1)
    {
        if (!q.empty() && q.pop(item))
            switch (item.type)
            {
            case Item::Type::Clear:
                port.write("C\n");
                break;

            case Item::Type::Set_status:
                port.write(fmt::format("TE,2,{},{}\n", static_cast<int>(item.color), item.s));
                break;
                
            case Item::Type::Show_message:
                port.write(fmt::format("TE,2,{},{}\n", static_cast<int>(item.color), item.s));
                clear_at = util::now() + item.dur;
                //!! clear
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
            port.write("TE,2,,\n");
        }
    }
}
