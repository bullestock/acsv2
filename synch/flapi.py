import requests, os, yaml, sys
from pathlib import Path
from django.contrib.auth.models import User
import logging

BASE_DIR = Path(__file__).resolve().parent.parent

def _get_fl_json():
    logger = logging.getLogger("django")
    logger.info("ForeningLet synch starting")
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
    members = _get_fl_json()
    if members is not None:
        try:
            yml_dir = os.path.join(BASE_DIR, 'synch')
            with open(yml_dir + '/secret.yml', encoding='utf-8', errors='replace') as ymlfile:
                yml = ymlfile.read()
            secret = yaml.safe_load(yml)
            activity_ids = secret['activity_ids']
            logger.info("Activity IDs: %s" % activity_ids)
            active_members = []
            updated_members = []
            added_members = []
            nologin_members = []
            excluded_members = []
            for m in members:
                id = int(m["MemberId"])
                number = m["MemberNumber"]
                first_name = m["FirstName"]
                last_name = m["LastName"]
                login = m["MemberField6"]
                name = first_name + " " + last_name
                activities = m["Activities"]
                activities = int(activities) if activities.isdecimal() else None # stupid sexy ForeningLet
                #logger.info("Member {0} activities: {1}".format(name, activities))
                if not activities in activity_ids:
                    logger.info("Member {0} (ID {1}) has no activities".format(name, id))
                    excluded_members.append(name)
            logger.info("Processed %d members" % len(members))
            # new_forecast = Forecast()
            # new_forecast.temperatue = temp_in_celsius
            # new_forecast.save()
            # logger.info("saving...\n" + new_forecast)
        except Exception as e:
            logger.info("Exception: %s" % e)
            pass
