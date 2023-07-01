#include "lock.h"
#include "logger.h"
#include "controller.h"
#include "util.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

static constexpr auto GPIO_PIN_LOCK = 0; // PA00

Lock::Lock()
    : thread([this](){ thread_body(); })
{
    util::unexport_pin(GPIO_PIN_LOCK);
    set_pin_output(GPIO_PIN_LOCK);
}

Lock::~Lock()
{
    stop = true;
    if (thread.joinable())
        thread.join();
    util::unexport_pin(GPIO_PIN_LOCK);
}

void Lock::set_pin_output(int pin)
{
    Logger::instance().log_verbose(fmt::format("set pin {} to output", pin));
    int fd = open("/sys/class/gpio/export", O_WRONLY);
    if (fd == -1)
        fatal_error("Unable to open /sys/class/gpio/export");

    const auto name = fmt::format("{}", pin);
    if (write(fd, name.c_str(), name.size()) != name.size())
        fatal_error("Error writing to /sys/class/gpio/export");

    close(fd);

    const auto dir = fmt::format("/sys/class/gpio/gpio{}/direction", pin);
    fd = open(dir.c_str(), O_WRONLY);
    if (fd == -1)
        fatal_error(fmt::format("Unable to open {}: {}", dir, errno));
    if (write(fd, "out", 3) != 3)
        fatal_error(fmt::format("Error writing to {}", dir));
    close(fd);
}

void Lock::set_pin(int pin, bool value)
{
    const auto name = fmt::format("/sys/class/gpio/gpio{}/value", pin);
    int fd = open(name.c_str(), O_WRONLY);
    if (fd == -1)
        fatal_error(fmt::format("Unable to open {}: {}", name, errno));
    char ch = value ? '1' : '0';
    if (write(fd, &ch, 1) != 1)
        fatal_error(fmt::format("Error writing to {}: {}", name, errno));
    close(fd);
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
    set_pin(GPIO_PIN_LOCK, desired_state == State::open);
    state = desired_state;
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
