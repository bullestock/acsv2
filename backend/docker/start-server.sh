#!/usr/bin/env bash
(cd acsv3; gunicorn acs.wsgi --user www-data --bind 0.0.0.0:8010 --workers 3 --preload 2> gunicorn.log) &
nginx -g "daemon off;"
