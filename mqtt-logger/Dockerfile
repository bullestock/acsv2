FROM python:3.12-slim

ARG GIT_COMMIT=unknown
LABEL git-commit=$GIT_COMMIT

WORKDIR /opt/service

RUN pip install --no-cache-dir paho-mqtt && \
    mkdir -p /opt/service/logs

COPY mqtt_logger.py /opt/service/

ENTRYPOINT ["python3", "/opt/service/mqtt_logger.py"]