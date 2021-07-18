from pathlib import Path
from machines.models import Machine
from members.models import Member
import logging
import traceback
import csv
import os

BASE_DIR = Path(__file__).resolve().parent.parent

def copy_users():
    logger = logging.getLogger("django")
    logger.info("Card synch starting")
    csv_path = os.path.join(BASE_DIR, 'users.csv')
    count = 0
    with open(csv_path) as csv_file:
        csv_reader = csv.reader(csv_file, delimiter=',')
        for row in csv_reader:
            fl_id = int(row[0]) if row[0].isdecimal() else None
            if not fl_id:
                continue
            card_id = row[1]
            if card_id == '\\N':
                continue
            #logger.info("%d %s" % (fl_id, card_id))
            try:
                u = Member.objects.get(fl_id=fl_id)
            except Member.DoesNotExist:
                continue
            u.card_id = card_id
            u.save()
            count = count + 1
    logger.info("Updated %d cards" % count)
