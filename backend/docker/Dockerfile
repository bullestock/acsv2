FROM python:3.13-bookworm

ARG GIT_COMMIT=unknown
LABEL git-commit=$GIT_COMMIT

RUN apt-get update && apt-get install nginx python3-mysqldb mariadb-client -y --no-install-recommends
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

WORKDIR /opt/app/acsv3
RUN DJANGO_SECRET_KEY=dummy DB_PASSWORD=dummy python manage.py collectstatic -v 2 --noinput

WORKDIR /opt/app

EXPOSE 8020
STOPSIGNAL SIGTERM
ENV GIT_COMMIT=$GIT_COMMIT
CMD ["/opt/app/start-server.sh"]
