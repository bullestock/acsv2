from django.conf import settings
from django.db import models
from django.contrib.auth.models import User

class Log(models.Model):
    user = models.ForeignKey(settings.AUTH_USER_MODEL, on_delete=models.CASCADE)
    text = models.CharField(max_length=200)
    stamp = models.DateTimeField('timestamp')
