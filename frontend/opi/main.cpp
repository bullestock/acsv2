#include "cardcache.h"
#include "slack.h"

#include <iomanip>
#include <iostream>

int main()
{
#if 0
    Card_cache cc;

    Card_cache::Card_id fl15 = 0x13006042CF;
    std::cout << "fl15: " << cc.has_access(fl15) << std::endl;
    std::cout << "fl15: " << cc.has_access(fl15) << std::endl;
#endif
    Slack_writer slack(true, true);
    slack.send_message(":bjarne: Hello from C++");
}
