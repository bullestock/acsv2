#pragma once

#include <string>

class Card_reader;
class Display;
class Lock;
class Slack_writer;

bool run_test(const std::string& arg);

bool run_test(const std::string& arg,
              Slack_writer& slack,
              Display& display,
              Card_reader& reader,
              Lock& lock);

