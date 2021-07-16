from datetime import datetime
from apscheduler.schedulers.background import BackgroundScheduler
from synch import flapi

def start():
    scheduler = BackgroundScheduler()
    scheduler.add_job(flapi.update_fl, 'interval', minutes=10)
    scheduler.start()
