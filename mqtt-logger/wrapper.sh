crond -f -l 8 -d 8 -L /dev/stdout
echo $ > /var/run/app.pid
exec /opt/service/service.sh
