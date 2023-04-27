#include "foreninglet.h"

#include <fmt/core.h>
#include <fmt/chrono.h>

#include <restclient-cpp/restclient.h>
#include <restclient-cpp/connection.h>

#include <fstream>
#include <iomanip>
#include <iostream>

constexpr auto URL = "https://foreninglet.dk/api/member/id/{}/?version=1";

ForeningLet& ForeningLet::instance()
{
    static ForeningLet the_instance;
    return the_instance;
}

ForeningLet::ForeningLet()
    : q(25),
      thread([this](){ thread_body(); })
{
    std::ifstream is("./foreninglet-credentials");
    std::getline(is, forening_let_user);
    std::getline(is, forening_let_password);
    if (forening_let_user.empty() || forening_let_password.empty())
    {
        std::cerr << "Missing ForeningLet credentials\n";
        exit(1);
    }
}

ForeningLet::~ForeningLet()
{
    stop = true;
    if (thread.joinable())
        thread.join();
}

void ForeningLet::update_last_access(int user_id, const util::time_point& timestamp)
{
    const auto stamp = fmt::format("{:%Y-%m-%d %H:%M:%S}", fmt::gmtime(timestamp));
    Item item{ user_id };
    strncpy(item.stamp, stamp.c_str(), std::min<size_t>(Item::STAMP_SIZE, stamp.size()));
    q.push(item);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

void ForeningLet::thread_body()
{
    Item item;
    while (!stop)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        try
        {
            if (q.empty() || !q.pop(item))
                continue;

            RestClient::Connection conn(fmt::format(URL, item.user_id));
            conn.AppendHeader("Content-Type", "application/json");
            util::json members;
            members["field1"] = item.stamp;
            util::json payload;
            payload["members"] = members;
            const auto resp = conn.put("", payload.dump());
            if (resp.code != 200)
            {
                std::cout << "ForeningLet log error code " << resp.code << std::endl;
                std::cout << "- body: " << resp.body << std::endl;
            }
        }
        catch (const std::exception& e)
        {
            std::cout << "Exception in foreninglet: " << e.what() << std::endl;
        }
    }
}
