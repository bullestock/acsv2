mosquitto_sub -h mqtt.hal9k.dk -t hal9k/acs/log/# -v -p 8883 | ts >> /opt/service/logs/acs
