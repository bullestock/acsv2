from django.db import models

class UnknownCard(models.Model):
    card_id = models.CharField(max_length=10)
    stamp = models.DateTimeField('timestamp', auto_now_add=True)
