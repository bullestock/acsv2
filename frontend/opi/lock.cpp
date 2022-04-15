#include "lock.h"
#include "controller.h"
#include "util.h"

Lock::Lock(serialib& p)
    : port(p),
      thread([this](){ thread_body(); })
{
}

Lock::Status Lock::get_status() const
{
    return { state, door_is_open, handle_is_raised };
}

bool Lock::set_state(Lock::State desired_state)
{
    // TODO
    state = desired_state;

    return true;
}

std::string Lock::get_error_msg() const
{
    return last_error;
}

void Lock::thread_body()
{
    while (1)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (!port.write("status\n"))
        {
            std::cout << "Lock: Write status failed\n";
            continue;
        }
        std::string line;
        const int nof_bytes = port.readString(line, '\n', 50, 100);
        // OK: status unknown open lowered 0
        Controller::instance().log("Lock reply: {}", util::strip(line));
        const auto parts = util::split(line, " ");
        if (parts.size() != 6 || parts[0] != "OK:" || parts[1] != "status")
            continue;
        if (parts[2] == "unknown")
            state = State::unknown;
        else if (parts[2] == "locked")
            state = State::locked;
        else if (parts[2] == "unlocked")
            state = State::open;
        else
            Controller::instance().log("Bad lock status");
        if (parts[3] == "open")
            door_is_open = true;
        else if (parts[3] == "closed")
            door_is_open = false;
        else
            Controller::instance().log("Bad door status");
        if (parts[4] == "lowered")
            handle_is_raised = false;
        else if (parts[4] == "raised")
            handle_is_raised = true;
        else
            Controller::instance().log("Bad handle status");
        int pos = 0;
        if (util::from_string(parts[5], pos))
            encoder_pos = pos;
        else
            Controller::instance().log("Bad encoder position");
    }
}

