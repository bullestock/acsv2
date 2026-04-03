FROM efrecon/mqtt-client

ARG GIT_COMMIT=unknown
LABEL git-commit=$GIT_COMMIT

COPY ./service.sh /opt/service/
COPY ./wrapper.sh /opt/service/

USER root

RUN apk add dcron logrotate moreutils

# Configure logrotate
COPY my-logrotate.conf /etc/logrotate.d/

# Set up cron to run logrotate hourly
RUN echo '0 * * * * /usr/sbin/logrotate -f /etc/logrotate.conf >/proc/1/fd/1 2>/proc/1/fd/2' > /etc/cron.d/logrotate-cron \
    && chmod 0644 /etc/cron.d/logrotate-cron \
    && crontab /etc/cron.d/logrotate-cron

RUN mkdir -p /opt/service/logs \
    && touch /var/log/logrotate.status \
    && chmod 644 /var/log/logrotate.status

ENTRYPOINT ["tini", "--"]
CMD ["sh", "wrapper.sh"]