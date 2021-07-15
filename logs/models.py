from django.db import models
from django.contrib.auth.models import User

class Log(models.Model):
    user = models.ForeignKey(User, on_delete=models.CASCADE)
    text = models.CharField(max_length=200)
    stamp = models.DateTimeField('timestamp')
