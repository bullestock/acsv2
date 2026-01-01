#!/bin/bash
set -e
serial=`cat docker-serial`
docker build -t bullestock/acs-mqtt-logger:$serial -t bullestock/acs-mqtt-logger:latest -f Dockerfile --build-arg GIT_COMMIT=`git rev-parse HEAD` .
docker push bullestock/acs-mqtt-logger:$serial
docker push bullestock/acs-mqtt-logger:latest
expr $serial + 1 > docker-serial
