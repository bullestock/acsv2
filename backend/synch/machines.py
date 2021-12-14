from pathlib import Path
from machines.models import Machine
import csv
import logging
import os
import traceback

BASE_DIR = Path(__file__).resolve().parent.parent

def copy_machines():
    logger = logging.getLogger("django")
    logger.info("Machines synch starting")
    csv_path = os.path.join(BASE_DIR, 'machines.csv')
    count = 0
    with open(csv_path) as csv_file:
        csv_reader = csv.reader(csv_file, delimiter=',')
        for row in csv_reader:
            name = row[0]
            try:
                new_m = Machine.objects.get(name=name)
            except Machine.DoesNotExist:
                new_m = None
            if new_m:
                logger.info("Machine {0} already exists".format(name))
            else:
                new_m = Machine(name=name, apitoken = row[1])
                new_m.save()
                count = count + 1
    logger.info("Added %d machines" % count)
