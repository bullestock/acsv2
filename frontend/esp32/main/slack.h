#pragma once

#include <string>

class Slack_writer
{
public:
    Slack_writer();

    void set_token(const std::string& token);
    
    void set_params(bool active,
                    bool test_mode);

    void announce_open();

    void announce_closed();

    void set_status(const std::string& status, bool include_general = false);

    void send_message(const std::string& message, bool include_general = false);

private:
    void send_to_channel(const std::string& channel,
                         const std::string& message);

    bool is_active = false;
    bool is_test_mode = false;
    std::string last_status;
    std::string api_token;
};
