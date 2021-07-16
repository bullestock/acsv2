from pathlib import Path
from machines.models import Machine
import logging
import traceback

BASE_DIR = Path(__file__).resolve().parent.parent

MACHINES = (
    ("Door","i02jt8ahaqk8s7txyvnrymzp5dmlvn3xj1v6i6gh3jccv27ngqc8nvli812dpop0"),
    ("Bungard","0caa17988bfcdaf177e44037b069edd6c81a0b0522a41d005473df4ce74947a7"),
    ("Lasersaur","1c824b9d79e73074610d4e2b0097ccb461b66e5f32de091060b7e32b35ec0d53"),
    ("Huvema","e0f250d6ba644ac7f4037c9ebd5fa20eb5eec29b9a2e9eef0a1ec7a8a54dfda7"),
    ("Vesuv Royal","aadd3f4bb42a133930495ac59361beba46066f18c67a2590d51b115a4a4a7b3c"),
    ("Lulzbot Taz","907d09ca5883f70ccb6bdad18cd17a791780e677253daf15d7b7930d0cca74e9"),
    ("Camera","e2d042e7ade2fb6b392570283bb9d125f19c52ca85a2be667f91dfef0dde8e0d"),
    ("Metalbåndsav","1ec88172151d2226ef56cdf57c64eeb955dd0dd6bf015a1d4489c00741b42401"),
    ("Ultimaker","a82cca9796b01ae39c07d00cfb97d36acf74b69a161d8df64483f16b50dfd280"),
    ("Laden","5cc9fa0f507f6739b77110e937756eb680e30a0d4609894307435fa8f0198444"),
)

def copy_machines():
    logger = logging.getLogger("django")
    logger.info("Machines synch starting")
    for m in MACHINES:
        name = m[0]
        try:
            new_m = Machine.objects.get(name=name)
        except Machine.DoesNotExist:
            new_m = None
        if new_m:
            logger.info("Machine {0} already exists".format(name))
        else:
            new_m = Machine(name=name, apitoken = m[1])
            new_m.save()
