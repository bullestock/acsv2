#include "display.h"

Display::Display(serialib& p)
    : port(p),
      thread(&Display::thread_body, this),
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

void Display::set_status(const std::string& text)
{
    Item item{ Item::Type::Set_status };
    strncpy(item.s, text.c_str(), std::min(Item::MAX_SIZE, text.size()));
    q.push(item);
}

void Display::show_message(const std::string& text,
                           util::duration dur)
{
    Item item{ Item::Type::Show_message, "", dur };
    strncpy(item.s, text.c_str(), std::min(Item::MAX_SIZE, text.size()));
    q.push(item);
}

void Display::thread_body(Display* self)
{
    auto& port = self->port;
    auto& q = self->q;
    Item item;
    while (1)
        if (q.pop(item))
            switch (item.type)
            {
            case Item::Type::Clear:
                port.write("C\n");
                break;

            default:
                std::cout << "Item type " << int(item.type) << " not handled\n";
                break;
            }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
}
