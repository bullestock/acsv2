#!/bin/bash
ruby /home/torsten/acsv2/ui/init.rb | tee /home/torsten/acsv2/ui.log
ruby /home/torsten/acsv2/ui/ui.rb | tee -a /home/torsten/acsv2/uiv2.log
