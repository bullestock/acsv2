import requests, os, yaml, sys
from pathlib import Path
from django.contrib.auth.models import User
from members.models import Member
import logging
import traceback
import pprint

BASE_DIR = Path(__file__).resolve().parent.parent

def _get_fl_json():
    logger = logging.getLogger("django")
    logger.info("ForeningLet synch starting")
    yml_dir = os.path.join(BASE_DIR, 'synch')
    with open(yml_dir + '/secret.yml', encoding='utf-8', errors='replace') as ymlfile:
     yml = ymlfile.read()
    secret = yaml.safe_load(yml)
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
            active_members = []
            updated_members = []
            added_members = []
            excluded_members = []
            for m in members:
                number = int(m["MemberNumber"])
                first_name = m["FirstName"]
                last_name = m["LastName"]
                fl_login = "FL{}".format(number)
                login = m["MemberField6"]
                if len(login) == 0:
                    login = fl_login
                name = first_name + " " + last_name
                activities = m["Activities"]
                activities = int(activities) if activities.isdecimal() else None # stupid sexy ForeningLet
                #logger.info("Member {0} activities: {1}".format(name, activities))
                if not activities in activity_ids:
                    logger.info("Member {0} (ID {1}) has no activities".format(name, number))
                    excluded_members.append(number)
                    continue
                try:
                    u = Member.objects.get(username=login)
                except Member.DoesNotExist:
                    u = None
                if u:
                    #logger.info("Member {0} (ID {1}) already exists".format(name, number))
                    updated_members.append(number)
                else:
                    logger.info("Member {0} does not exist".format(name))
                    if login != fl_login:
                        try:
                            fu = Member.objects.get(username=fl_login)
                            logger.info("Deleting old member {0} {1}".format(fu.first_name,
                                                                             fu.last_name))
                            fu.delete()
                        except Member.DoesNotExist:
                            pass
                    added_members.append(number)
                    u = Member.objects.create_user(login)
                    u.set_password(None)
                u.is_active = True
                u.fl_id = number
                u.first_name = first_name
                u.last_name = last_name
                #logger.info("Member %d is active" % number)
                active_members.append(number)
                u.save()

            # Deactivate remaining members
            for u in Member.objects.all():
                #logger.info("User %s" % pprint.pformat(u.__dict__, depth=10))
                try:
                    if u.fl_id:
                        if not u.fl_id in active_members:
                            logger.info("Member {0} {1} (ID {2}) is no longer active".format(u.first_name,
                                                                                             u.last_name,
                                                                                             u.fl_id))
                            u.is_active = False
                            u.save()
                except Exception as e:
                    logger.info("Exception: {0} {1}".format(e, traceback.format_exc()))

            # Output statistics
            logger.info("Processed %d members" % len(members))
            logger.info("Updated {0}, added {1}, excluded {2}.".format(len(updated_members),
                                                                       len(added_members),
                                                                       len(excluded_members)))
            logger.info("Total {0} active members.".format(len(active_members)))
        except Exception as e:
            logger.info("Exception: {0} {1}".format(e, traceback.format_exc()))
            pass
