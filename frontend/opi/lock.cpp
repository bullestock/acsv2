#include "lock.h"

Lock::Lock(serialib& p)
    : port(p)
{
}

Lock::State Lock::get_state() const
{
    return state;
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

