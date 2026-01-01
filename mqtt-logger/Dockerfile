FROM efrecon/mqtt-client

ARG GIT_COMMIT=unknown
LABEL git-commit=$GIT_COMMIT

COPY ./service.sh /opt/service/
WORKDIR /opt/service

USER root
RUN mkdir -p /opt/service/logs

ENTRYPOINT ["tini"]
CMD ["service.sh"]