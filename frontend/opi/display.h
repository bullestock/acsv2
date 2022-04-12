#pragma once

#include "serialib.h"
#include "util.h"

#include <thread>

#include <boost/lockfree/queue.hpp>

class Display
{
public:
    enum class Color {
        red,
        green,
        blue,
        white,
        gray,
        yellow,
        cyan,
        purple,
    };

    Display(serialib&);

    ~Display();

    /// Clear entire display
    void clear();

    /// Set status
    void set_status(const std::string& text,
                    Color col = Color::white);

    /// Show message for specified time
    void show_message(const std::string& text,
                      Color col = Color::white,
                      util::duration dur = std::chrono::seconds(5));                      

private:
    serialib& port;
    std::thread thread;
    struct Item {
        enum class Type {
            Clear,
            Set_status,
            Show_message,
        };
        Type type = Type::Clear;
        Color color = Color::white;
        static const int MAX_SIZE = 80;
        char s[MAX_SIZE+1];
        util::duration dur = std::chrono::seconds(0);
    };
    boost::lockfree::queue<Item> q;
    util::time_point clear_at;

    static void thread_body(Display*);
};
