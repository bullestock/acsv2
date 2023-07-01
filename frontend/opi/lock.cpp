#include "lock.h"
#include "logger.h"
#include "controller.h"
#include "util.h"

Lock::Lock()
    : thread([this](){ thread_body(); })
{
}

Lock::~Lock()
{
    stop = true;
    if (thread.joinable())
        thread.join();
}

Lock::Status Lock::get_status() const
{
    std::lock_guard<std::mutex> g(mutex);
    return { state, door_is_open };
}

bool Lock::set_state(Lock::State desired_state)
{
    std::lock_guard<std::mutex> g(mutex);
    if (state == desired_state)
        return true;
    //!! TODO
    return true;
}

void Lock::thread_body()
{
    int failures = 0;
    
    // Allow controller time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    while (!stop)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        {
            std::lock_guard<std::mutex> g(mutex);
        }
    }
}
