#include "controller.h"

#include "cardreader.h"
#include "display.h"
#include "lock.h"

Controller::Controller(Display& d,
                       Card_reader& r,
                       Lock& l)
    : display(d),
      reader(r),
      lock(l)
{
}

void Controller::run()
{
    while (1)
        ;
}
