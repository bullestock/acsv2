import requests, os, yaml, sys
from pathlib import Path
from django.contrib.auth.models import User
from members.models import Member
import logging
import pprint
import datetime
import traceback

BASE_DIR = Path(__file__).resolve().parent.parent

def _get_fl_json():
    logger = logging.getLogger("django")
    logger.info(f'SYNCH: ForeningLet synch starting {datetime.datetime.now()}')
    yml_dir = os.path.join(BASE_DIR, 'synch')
    user = os.environ.get('FL_USER')
    password = os.environ.get('FL_PASS')
    with open(yml_dir + '/foreninglet.yml', encoding='utf-8', errors='replace') as ymlfile:
        yml = ymlfile.read()
    foreninglet = yaml.safe_load(yml)
    getall_url = foreninglet['getall']
    url = "https://{0}:{1}@{2}".format(user, password, getall_url)
    r = requests.get(url)
    try:
        r.raise_for_status()
        return r.json()
    except Exception as e:
        logger.info("SYNCH: Exception: %s" % e)
        return None

      
def update_fl():
    logger = logging.getLogger("django")
    members = _get_fl_json()
    if members is not None:
        try:
            yml_dir = os.path.join(BASE_DIR, 'synch')
            with open(yml_dir + '/foreninglet.yml', encoding='utf-8', errors='replace') as ymlfile:
                yml = ymlfile.read()
            foreninglet = yaml.safe_load(yml)
            active_members = []
            updated_members = []
            added_members = []
            for m in members:
                id = int(m["MemberId"])
                number = int(m["MemberNumber"])
                first_name = m["FirstName"]
                last_name = m["LastName"]
                fl_login = "FL{}".format(number)
                login = m["MemberField6"]
                if len(login) == 0:
                    login = fl_login
                name = first_name + " " + last_name
                try:
                    u = Member.objects.get(username=login)
                except Member.DoesNotExist:
                    logger.info("SYNCH: Member {0} (ID {1}, username {2}) does not exist".format(name, number, login))
                    u = None
                if u:
                    #logger.info("SYNCH: Member {0} (ID {1}) already exists".format(name, number))
                    updated_members.append(number)
                else:
                    logger.info("SYNCH: Member {0} does not exist".format(name))
                    if login != fl_login:
                        try:
                            fu = Member.objects.get(username=fl_login)
                            logger.info("SYNCH: Deleting old member {0} {1}".format(fu.first_name,
                                                                             fu.last_name))
                            fu.delete()
                        except Member.DoesNotExist:
                            pass
                    added_members.append(number)
                    u = Member.objects.create_user(login)
                    u.set_password(None)
                if not u.is_active:
                    logger.info("SYNCH: Member %d is now active" % number)
                u.is_active = True
                u.fl_id = number
                u.fl_int_id = id
                u.first_name = first_name
                u.last_name = last_name
                active_members.append(number)
                u.save()

            # Deactivate remaining members
            for u in Member.objects.all():
                #logger.info("SYNCH: User %s" % pprint.pformat(u.__dict__, depth=10))
                try:
                    if u.fl_id:
                        if not u.fl_id in active_members:
                            if u.is_active:
                                logger.info(f'SYNCH: Member {u.first_name} {u.last_name} (ID {u.fl_id}) is no longer active')
                            u.is_active = False
                            u.save()
                except Exception as e:
                    logger.info("SYNCH: Exception: {0} {1}".format(e, traceback.format_exc()))

            # Output statistics
            logger.info(f'SYNCH: Ended {datetime.datetime.now()}. Got {len(members)}, processed {len(members)} ' +
                        f', updated {len(updated_members)}, added {len(added_members)}. ' +
                        f'Total {len(active_members)} active members.')
            Path('/opt/app/acsv2/monitoring/acs-sync-status').touch()
        except Exception as e:
            logger.info("SYNCH: Exception: {0} {1}".format(e, traceback.format_exc()))
            pass
