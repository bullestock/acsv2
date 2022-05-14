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
    if (thread.joinable())
        thread.join();
}

void Lock::set_logger(Logger& l)
{
    logger = &l;
}

Lock::Status Lock::get_status() const
{
    return { state, door_is_open, handle_is_raised, encoder_pos };
}

bool Lock::set_state(Lock::State desired_state)
{
    // TODO
    state = desired_state;

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
        if (logger)
            logger->log_verbose(fmt::format("Lock reply: {}", util::strip(line)));
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
    }
}

