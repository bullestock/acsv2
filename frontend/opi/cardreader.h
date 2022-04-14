#pragma once

#include "serialib.h"

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

class Card_reader
{
public:
    enum class Pattern {
        none,
        ready,
        enter,
        open,
        warn_closing,
        error,
        no_entry,
        wait,
    };
    
    Card_reader(serialib&);

    void set_pattern(Pattern);

    void start_beep();
    
    void stop_beep();

    std::string get_and_clear_card_id();
    
private:
    void thread_body();
    
    serialib& port;
    std::thread thread;
    std::mutex mutex;
    std::string card_id;
    std::atomic<bool> beep_on = false;
    std::atomic<Pattern> pattern = Pattern::none;
};
