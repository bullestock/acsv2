#include "lock.h"
#include "logger.h"
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
    const auto command = (desired_state == State::open) ? "unlock" : "lock";
    if (!write(command))
    {
        std::cout << fmt::format("Lock: Write '{}' failed\n", command);
        return false;
    }
    const auto line = get_reply(command);
    const auto parts = util::split(line, " ");
    if (parts.size() != 2 || parts[0] != "OK:")
    {
        Logger::instance().log(fmt::format("ERROR: Cannot lock the door: {}", line));
        return false;
    }
    return true;
}

bool Lock::calibrate()
{
    std::lock_guard<std::mutex> g(mutex);
    const auto cmd = "calibrate";
    if (!write(cmd))
    {
        std::cout << "Lock: Write 'calibrate' failed\n";
        return false;
    }
    const auto reply = get_reply(cmd);
    // reply: OK: locked 0-20 Unlocked 69-89
    const auto parts = util::split(reply, " ");
    if (parts.size() != 5)
    {
        Logger::instance().fatal_error(fmt::format("ERROR: Bad 'calibrate' reply from lock: {}", reply));
        return false;
    }
    const auto locked = util::split(parts[2], "-");
    if (locked.size() != 2)
    {
        Logger::instance().fatal_error(fmt::format("ERROR: Bad 'calibrate' reply from lock (locked): {}", reply));
        return false;
    }
    const auto unlocked = util::split(parts[4], "-");
    if (unlocked.size() != 2)
    {
        Logger::instance().fatal_error(fmt::format("ERROR: Bad 'calibrate' reply from lock (unlocked): {}", reply));
        return false;
    }
    util::from_string(locked[0], locked_range.first);
    util::from_string(locked[1], locked_range.second);
    util::from_string(unlocked[0], unlocked_range.first);
    util::from_string(unlocked[1], unlocked_range.second);
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

std::string Lock::get_reply(const std::string& cmd)
{
    std::string line;
    while (1)
    {
        port.readString(line, '\n', 50, 100);
        if (line.empty())
            return line;
        if (line.substr(0, 5) == std::string("DEBUG"))
        {
            Logger::instance().log_verbose(line);
            continue;
        }
        line = util::strip(line);
        if (line == cmd)
            // echo
            continue;
        Logger::instance().log_verbose(fmt::format("LOCK: R '{}'", line));
        return line;
    }
}

void Lock::thread_body()
{
    while (!stop)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        {
            std::lock_guard<std::mutex> g(mutex);
            const auto cmd = "status";
            if (!write(cmd))
            {
                std::cout << "Lock: Write status failed\n";
                continue;
            }
            // OK: status unknown open lowered 0
            std::string line;
            do
                line = get_reply(cmd);
            while (line == "status");
            Logger::instance().log_verbose(fmt::format("Lock status reply: {}", util::strip(line)));
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
                Logger::instance().log("Bad lock status");
            if (parts[3] == "open")
                door_is_open = true;
            else if (parts[3] == "closed")
                door_is_open = false;
            else
                Logger::instance().log("Bad door status");
            if (parts[4] == "lowered")
                handle_is_raised = false;
            else if (parts[4] == "raised")
                handle_is_raised = true;
            else
                Logger::instance().log("Bad handle status");
            int pos = 0;
            if (util::from_string(parts[5], pos))
                encoder_pos = pos;
            else
                Logger::instance().log("Bad encoder position");
        } // release lock
    }
}

bool Lock::write(const std::string& s)
{
    Logger::instance().log_verbose(fmt::format("LOCK: W '{}'", s));
    port.flushReceiver();
    return port.write(s + "\n");
}
