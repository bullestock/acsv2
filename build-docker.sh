#!/bin/bash
set -e
serial=`cat docker-serial`
pushd backend
docker build -t bullestock/acsv3:$serial -f docker/Dockerfile --build-arg GIT_COMMIT=`git rev-parse HEAD` --build-arg BUILD_DATE="`date '+%Y%m%d %H:%M'`" --build-arg VERSION_NUMBER=$serial .
docker tag bullestock/acsv3:$serial bullestock/acsv3:latest
docker push bullestock/acsv3:$serial
docker push bullestock/acsv3:latest
popd
expr $serial + 1 > docker-serial
