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

    enum class Sound {
        none,
        warning,
        uncalibrated
    };
    
    Card_reader(serialib&);

    void set_pattern(Pattern);

    void set_sound(Sound);

    std::string get_and_clear_card_id();
    
private:
    void thread_body();
    
    serialib& port;
    std::thread thread;
    std::mutex mutex;
    std::string card_id;
    std::atomic<Sound> sound = Sound::none;
    std::atomic<Pattern> pattern = Pattern::none;
};
