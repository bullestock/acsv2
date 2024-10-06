#!/bin/bash
set -e
serial=`cat docker-serial`
docker build -t bullestock/acsv3:$serial -t bullestock/acsv3:latest -f docker/Dockerfile --build-arg GIT_COMMIT=`git rev-parse HEAD` --build-arg VERSION_NUMBER=$serial .
docker push bullestock/acsv3:$serial
docker push bullestock/acsv3:latest
expr $serial + 1 > docker-serial
