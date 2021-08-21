from pathlib import Path
from machines.models import Machine
from members.models import Member
import logging
import traceback
import csv
import os

'''
from synch import user_machines
user_machines.import_machines()
'''

BASE_DIR = Path(__file__).resolve().parent.parent

def import_machines():
    logger = logging.getLogger("django")
    logger.info("Machine synch starting")
    csv_path = os.path.join(BASE_DIR, 'users_machines.csv')
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
            machine_name = row[1].strip()
            if len(machine_name) == 0:
                continue
            #logger.info("%d %s" % (fl_id, card_id))
            try:
                u = Member.objects.get(fl_id=fl_id)
            except Member.DoesNotExist:
                continue
            u.machine.add(Machine.objects.get(name=machine_name))
            u.save()
            count = count + 1
    logger.info("Updated %d machines" % count)
