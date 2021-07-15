import requests, os, yaml, sys
from pathlib import Path
from django.contrib.auth.models import User
import logging


def _get_fl_json():
    logger = logging.getLogger("django")
    logger.info("ForeningLet synch starting")
    BASE_DIR = Path(__file__).resolve().parent.parent
    logger.info("base %s" % BASE_DIR)
    yml_dir = os.path.join(BASE_DIR, 'synch')
    with open(yml_dir + '/secret.yml', encoding='utf-8', errors='replace') as ymlfile:
     yml = ymlfile.read()
    secret = yaml.safe_load(yml)
    logger.info("loaded")
    creds = secret['credentials']
    user = creds[0]
    password = creds[1]
    getall_url = secret['getall']
    url = "https://{0}:{1}@{2}".format(user, password, getall_url)
    logger.info("url: %s" % url)
    r = requests.get(url)
    try:
        r.raise_for_status()
        return r.json()
    except Exception as e:
        logger.info("Exception: %s" % e)
        return None

      
def update_fl():
    logger = logging.getLogger("django")
    json = _get_fl_json()
    if json is not None:
        try:
            # new_forecast = Forecast()
            # new_forecast.temperatue = temp_in_celsius
            # new_forecast.save()
            # logger.info("saving...\n" + new_forecast)
            logger.info("saving")
        except:
            pass
