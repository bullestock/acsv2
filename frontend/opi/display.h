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
        orange,
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

    /// Show information in small font
    void show_info(int line,
                   const std::string& text,
                   Color col = Color::white);

private:
    std::string get_reply(const std::string& cmd);
    
    void thread_body();

    void do_show_message(const std::string& msg, Color color);

    void do_show_info(int line,
                      const std::string& text,
                      Color color);
    
    serialib& port;
    std::thread thread;
    bool stop = false;
    struct Item {
        enum class Type {
            Clear,
            Set_status,
            Show_message,
            Show_info,
        };
        static const int MAX_SIZE = 80;
        Type type = Type::Clear;
        Color color = Color::white;
        char s[MAX_SIZE+1];
        int line_no = 0;
        util::duration dur = std::chrono::seconds(0);
    };
    boost::lockfree::queue<Item> q;
    util::time_point clear_at;
};
