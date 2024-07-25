#pragma once

#include <deque>
#include <mutex>
#include <string>

#include "esp_http_client.h"

extern "C" void slack_task(void*);

/// Slack_writer singleton
class Slack_writer
{
public:
    static Slack_writer& instance();

    void set_token(const std::string& token);
    
    void set_params(bool test_mode);

    void set_status(const std::string& status, bool include_general = false);

    struct Channels
    {
        bool general = false;
        bool monitoring = true;
        bool info = false;

        // workaround for https://gcc.gnu.org/bugzilla/show_bug.cgi?id=96645
        // this approach means that Channels is still an aggregate
        static Channels defaults()
        {
            return {};
        }
    };
    
    void send_message(const std::string& message,
                      Channels channels = Channels::defaults());

private:
    struct Item {
        std::string channel;
        std::string message;
    };

    Slack_writer() = default;

    ~Slack_writer() = default;

    void send_to_channel(const std::string& channel,
                         const std::string& message);
    
    void thread_body();
    
    void do_post(esp_http_client_handle_t client,
                 const Item& item);

    std::deque<Item> q;
    std::mutex mutex;
    bool is_test_mode = false;
    std::string last_status;
    std::string api_token;

    friend void slack_task(void*);
};

// Local Variables:
// compile-command: "cd .. && idf.py build"
// End:
