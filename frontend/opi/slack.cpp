#include "slack.h"
#include "util.h"

#include <restclient-cpp/restclient.h>
#include <restclient-cpp/connection.h>

#include <fstream>
#include <iomanip>
#include <iostream>

constexpr auto BASE_URL = "https://slack.com/api/";

Slack_writer::Slack_writer(bool active, bool test_mode)
    : is_active(active),
      is_test_mode(test_mode)
{
    std::ifstream is("./slack-token");
    std::getline(is, api_token);
    if (api_token.empty())
        std::cerr << "Missing Slack API token\n";
}

void Slack_writer::announce_open()
{
    set_status(":tada: The space is now open!", true);
}

void Slack_writer::announce_closed()
{
    set_status(":sad_panda2: The space is no longer open", true);
}

void Slack_writer::set_status(const std::string& status, bool include_general)
{
    if (status != last_status)
        send_message(status, include_general);
}

void Slack_writer::send_message(const std::string& message, bool include_general)
{
    last_status = message;
    send_to_channel(is_test_mode ? "testing" : "monitoring", message);
    if (include_general && !is_test_mode)
        send_to_channel("general", message);
}

void Slack_writer::send_to_channel(const std::string& channel,
                                   const std::string& message)
{
    if (!is_active || api_token.empty())
    {
        std::cout << "#" << channel << ": " << message << std::endl;
        return;
    }
    RestClient::Connection conn(BASE_URL);
    conn.AppendHeader("Content-Type", "application/json");
    conn.AppendHeader("Authorization", std::string("Bearer ") + api_token);
    util::json payload;
    payload["channel"] = channel;
    payload["icon_emoji"] = ":panopticon:";
    payload["parse"] = "full";
    payload["text"] = message;
    const auto resp = conn.post("/chat.postMessage", payload.dump());
    if (resp.code == 200)
        return;
    std::cout << "Slack error code " << resp.code << std::endl;
    std::cout << "- body: " << resp.body << std::endl;
}

