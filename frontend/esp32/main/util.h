#pragma once

#include <sstream>
#include <string>
#include <vector>

#include "lcd_api.h"

/// Strip trailing/leading whitespace
std::string strip(const std::string&);

void split(const std::string& s,
           const std::string& delim,
           std::vector<std::string>& v);

template <typename T> bool from_string(const std::string& s, T& val)
{
   T temp;
   std::stringstream ss(s);
   if (!(ss >> temp))
       return false;
   val = temp;
   return true;
}

void update_spinner(TFT_t& dev);
