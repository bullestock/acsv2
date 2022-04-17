#!/bin/bash
set -e
serial=`cat docker-serial`
docker build -t bullestock/acsv2:$serial -t bullestock/acsv2:latest -f docker/Dockerfile --build-arg GIT_COMMIT=`git rev-parse HEAD` --build-arg VERSION_NUMBER=$serial .
docker push bullestock/acsv2:$serial
docker push bullestock/acsv2:latest
expr $serial + 1 > docker-serial
