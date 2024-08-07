#include "buttons.h"
#include "logger.h"
#include "util.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <iomanip>
#include <iostream>

#include <fmt/core.h>

static constexpr auto GPIO_PIN_RED = 16;
static constexpr auto GPIO_PIN_WHITE = 2;
static constexpr auto GPIO_PIN_GREEN = 15;
static constexpr auto GPIO_PIN_LEAVE = 13;

Buttons::Buttons()
{
    util::unexport_pin(GPIO_PIN_RED);
    util::unexport_pin(GPIO_PIN_WHITE);
    util::unexport_pin(GPIO_PIN_GREEN);
    util::unexport_pin(GPIO_PIN_LEAVE);
    set_pin_input(GPIO_PIN_RED);
    set_pin_input(GPIO_PIN_WHITE);
    set_pin_input(GPIO_PIN_GREEN);
    set_pin_input(GPIO_PIN_LEAVE);
}

Buttons::~Buttons()
{
    util::unexport_pin(GPIO_PIN_RED);
    util::unexport_pin(GPIO_PIN_WHITE);
    util::unexport_pin(GPIO_PIN_GREEN);
    util::unexport_pin(GPIO_PIN_LEAVE);
}

Buttons::Keys Buttons::read(bool log)
{
    return {
        !read_pin(GPIO_PIN_RED, log),
        !read_pin(GPIO_PIN_WHITE, log),
        false, /* !! green button does not work */ // !read_pin(GPIO_PIN_GREEN, log),
        !read_pin(GPIO_PIN_LEAVE, log)
    };
}

void Buttons::set_pin_input(int pin)
{
    Logger::instance().log_verbose(fmt::format("set pin {} to input", pin));
    int fd = open("/sys/class/gpio/export", O_WRONLY);
    if (fd == -1)
        fatal_error("Unable to open /sys/class/gpio/export");

    const auto name = fmt::format("{}", pin);
    if (write(fd, name.c_str(), name.size()) != name.size())
        fatal_error("Error writing to /sys/class/gpio/export");

    close(fd);

    const auto dir = fmt::format("/sys/class/gpio/gpio{}/direction", pin);
    fd = open(dir.c_str(), O_WRONLY);
    if (write(fd, "in", 2) != 2)
        fatal_error(fmt::format("Error writing to {}", dir));
    close(fd);
}

bool Buttons::read_pin(int pin, bool do_log)
{
    const auto name = fmt::format("/sys/class/gpio/gpio{}/value", pin);
    int fd = open(name.c_str(), O_RDONLY);
    if (fd == -1)
    {
        if (!do_log)
            return false;
        fatal_error(fmt::format("Unable to open {}: {}", name, errno));
    }
    char c;
    const auto n = ::read(fd, &c, 1);
    close(fd);
    if (n != 1)
    {
        if (!do_log)
            return false;
        fatal_error(fmt::format("Error reading from {}: {}", name, errno));
    }
    return c == '1';
}
