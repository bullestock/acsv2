from pathlib import Path
from machines.models import Machine
from members.models import Member
import logging
import traceback
import csv
import os

'''
from synch import cards
cards.import_cards()
'''

BASE_DIR = Path(__file__).resolve().parent.parent

def import_cards():
    logger = logging.getLogger("django")
    logger.info("Card synch starting")
    csv_path = os.path.join(BASE_DIR, 'users.csv')
    count = 0
    with open(csv_path) as csv_file:
        csv_reader = csv.reader(csv_file, delimiter=',')
        for row in csv_reader:
            if len(row) == 0:
                continue
            fl_id = row[0].strip()
            fl_id = int(fl_id) if fl_id.isdecimal() else None
            if not fl_id:
                continue
            card_id = row[1].strip()
            if len(card_id) == 0:
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
