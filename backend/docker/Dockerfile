FROM python:3.11-buster

ARG GIT_COMMIT=unknown
LABEL git-commit=$GIT_COMMIT

RUN apt-get update && apt-get install nginx python-mysqldb mariadb-client -y --no-install-recommends
COPY docker/nginx.default /etc/nginx/sites-available/default
RUN ln -sf /dev/stdout /var/log/nginx/access.log \
    && ln -sf /dev/stderr /var/log/nginx/error.log

RUN mkdir -p /opt/app
RUN mkdir -p /opt/app/acsv3/monitoring
COPY requirements.txt docker/start-server.sh /opt/app/
COPY ./ /opt/app/acsv3/
WORKDIR /opt/app
RUN pip install -r requirements.txt --no-cache-dir
RUN touch /opt/app/acsv3/monitoring/acs-sync-status
RUN chown -R www-data:www-data /opt/app

EXPOSE 8020
STOPSIGNAL SIGTERM
ENV GIT_COMMIT=$GIT_COMMIT
CMD ["/opt/app/start-server.sh"]
