from django.apps import AppConfig

import logging

class MachinesConfig(AppConfig):
    default_auto_field = 'django.db.models.BigAutoField'
    name = 'machines'

    def ready(self):
        logger = logging.getLogger("django")
        logger.info('Starting scheduler')
        from synch import updater
        updater.start()
