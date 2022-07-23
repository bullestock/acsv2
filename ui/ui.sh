#!/bin/bash
ruby /home/torsten/acsv2/ui/init.rb | tee /var/log/acsv2/ui.log
ruby /home/torsten/acsv2/ui/ui.rb | tee -a /var/log/acsv2/uiv2.log
