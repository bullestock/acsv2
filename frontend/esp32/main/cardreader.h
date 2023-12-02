#pragma once

#include <RDM6300.h>

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

extern "C" void card_reader_task(void*);

class Card_reader
{
public:
    using Card_id = RDM6300::Card_id;

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
        warn_closing
    };
    
    static Card_reader& instance();

    void set_pattern(Pattern);

    void set_sound(Sound);

    Card_id get_and_clear_card_id();
    
private:
    Card_reader() = default;

    ~Card_reader() = default;
    
    void thread_body();
    
    std::mutex mutex;
    Card_id card_id = 0;
    std::atomic<Sound> sound = Sound::none;
    std::atomic<Pattern> pattern = Pattern::none;

    friend void card_reader_task(void*);
};
