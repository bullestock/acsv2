#pragma once

class Card_reader;
class Display;
class Lock;

class Controller
{
public:
    Controller(Display& display,
               Card_reader& reader,
               Lock& lock);

    void run();
    
private:
    Display& display;
    Card_reader& reader;
    Lock& lock;
};
