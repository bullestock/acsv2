#include "cardcache.h"

#include <iomanip>
#include <iostream>

int main()
{
    Card_cache cc;

    Card_cache::Card_id fl15 = 0x13006042CF;
    std::cout << "fl15: " << cc.has_access(fl15) << std::endl;
}
