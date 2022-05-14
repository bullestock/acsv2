#include "lock.h"
#include "controller.h"
#include "util.h"

Lock::Lock(serialib& p)
    : port(p),
      thread([this](){ thread_body(); })
{
}

Lock::~Lock()
{
    stop = true;
    if (thread.joinable())
        thread.join();
}

void Lock::set_logger(Logger& l)
{
    logger = &l;
}

Lock::Status Lock::get_status() const
{
    std::lock_guard<std::mutex> g(mutex);
    return { state, door_is_open, handle_is_raised, encoder_pos };
}

bool Lock::set_state(Lock::State desired_state)
{
    bool is_unknown = false;
    {
        std::lock_guard<std::mutex> g(mutex);
        is_unknown = (state == State::unknown);
    }
    if (is_unknown)
    {
        if (!calibrate())
            return false;
    }
    std::lock_guard<std::mutex> g(mutex);
    if (state == desired_state)
        return true;
    const auto command = (desired_state == State::open) ? "unlock" : "unlock";
    if (!port.write(fmt::format("{}\n", command)))
    {
        std::cout << fmt::format("Lock: Write '{}' failed\n", command);
        return false;
    }
    const auto line = get_reply();
    const auto parts = util::split(line, " ");
    if (parts.size() != 2 || parts[0] != "OK:")
    {
        if (logger)
            logger->log(fmt::format("ERROR: Cannot lock the door: {}", line));
        return false;
    }
    return true;
}

bool Lock::calibrate()
{
#if 0
    // reply: OK: locked 0-20 Unlocked 69-89
    const auto parts = util::split(reply, " ");
    if (parts.size() != 5)
        fatal_error(fmt::format("ERROR: Bad 'calibrate' reply from lock: {}", reply));
    const auto locked = util::split(parts[2], "-");
    if (locked.size() != 2)
        fatal_error(fmt::format("ERROR: Bad 'calibrate' reply from lock (locked): {}", reply));
    const auto unlocked = util::split(parts[4], "-");
    if (unlocked.size() != 4)
        fatal_error(fmt::format("ERROR: Bad 'calibrate' reply from lock (unlocked): {}", reply));
    util::from_string(locked[0], locked_range.first);
    util::from_string(locked[1], locked_range.second);
    util::from_string(unlocked[0], unlocked_range.first);
    util::from_string(unlocked[1], unlocked_range.second);
#endif
    return true;
}

std::string Lock::get_error_msg() const
{
    return last_error;
}

std::pair<std::pair<int, int>, std::pair<int, int>> Lock::get_ranges() const
{
    return std::make_pair(locked_range, unlocked_range);
}

std::string Lock::get_reply()
{
    std::string line;
    while (1)
    {
        port.readString(line, '\n', 50, 100);
        if (line.empty())
            return line;
        if (line.substr(0, 5) == std::string("DEBUG"))
        {
            if (logger)
                logger->log_verbose(line);
        }
        else
            return util::strip(line);
    }
}

void Lock::thread_body()
{
    while (!stop)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        {
            std::lock_guard<std::mutex> g(mutex);
            if (!port.write("status\n"))
            {
                std::cout << "Lock: Write status failed\n";
                continue;
            }
            // OK: status unknown open lowered 0
            const auto line = get_reply();
            if (logger)
                logger->log_verbose(fmt::format("Lock status reply: {}", util::strip(line)));
            const auto parts = util::split(line, " ");
            if (parts.size() != 6 || parts[0] != "OK:" || parts[1] != "status")
                continue;
            if (parts[2] == "unknown")
                state = State::unknown;
            else if (parts[2] == "locked")
                state = State::locked;
            else if (parts[2] == "unlocked")
                state = State::open;
            else if (logger)
                logger->log("Bad lock status");
            if (parts[3] == "open")
                door_is_open = true;
            else if (parts[3] == "closed")
                door_is_open = false;
            else if (logger)
                logger->log("Bad door status");
            if (parts[4] == "lowered")
                handle_is_raised = false;
            else if (parts[4] == "raised")
                handle_is_raised = true;
            else if (logger)
                logger->log("Bad handle status");
            int pos = 0;
            if (util::from_string(parts[5], pos))
                encoder_pos = pos;
            else if (logger)
                logger->log("Bad encoder position");
        } // release lock
    }
}


