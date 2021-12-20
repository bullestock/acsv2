#!/bin/bash
/home/hal9k/.rbenv/shims/ruby /home/hal9k/acsv2/ui/init.rb | tee /var/log/acs/ui.log
/home/hal9k/.rbenv/shims/ruby /home/hal9k/acsv2/ui/ui.rb | tee -a /var/log/acs/uiv2.log
