#pragma once

class Buttons
{
public:
    struct Keys
    {
        bool red = false;
        bool white = false;
        bool green = false;
        bool leave = false;

        bool operator==(const Keys&) const = default;
    };

    static Keys read();
};
