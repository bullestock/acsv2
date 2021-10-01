FROM python:3.7-buster

ARG GIT_COMMIT=unknown
LABEL git-commit=$GIT_COMMIT

RUN apt-get update && apt-get install nginx python-mysqldb -y --no-install-recommends
COPY docker/nginx.default /etc/nginx/sites-available/default
RUN ln -sf /dev/stdout /var/log/nginx/access.log \
    && ln -sf /dev/stderr /var/log/nginx/error.log

RUN mkdir -p /opt/app
RUN mkdir -p /opt/app/pip_cache
RUN mkdir -p /opt/app/acsv2/monitoring
COPY requirements.txt docker/start-server.sh /opt/app/
COPY .pip_cache /opt/app/pip_cache/
COPY ./ /opt/app/acsv2/
WORKDIR /opt/app
RUN pip install -r requirements.txt --cache-dir /opt/app/pip_cache
RUN chown -R www-data:www-data /opt/app
RUN touch /opt/app/acsv2/monitoring/acs-sync-status
RUN chown /opt/app/acsv2/monitoring

EXPOSE 8020
STOPSIGNAL SIGTERM
CMD ["/opt/app/start-server.sh"]
