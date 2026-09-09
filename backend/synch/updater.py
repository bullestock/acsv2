import logging
import threading
import time

from django.db import close_old_connections

from synch import flapi


logger = logging.getLogger("django")
UPDATE_INTERVAL = 5 * 60
_thread = None
_thread_lock = threading.Lock()


def _run():
    while True:
        close_old_connections()
        try:
            flapi.update_fl()
        except Exception:
            logger.exception("SYNCH: updater failed")
        finally:
            close_old_connections()
        time.sleep(UPDATE_INTERVAL)


def start():
    global _thread
    with _thread_lock:
        if _thread is not None and _thread.is_alive():
            return
        _thread = threading.Thread(
            target=_run,
            name="acs-updater",
            daemon=True,
        )
        _thread.start()
