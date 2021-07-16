from django.conf import settings
from django.db import models
from django.contrib.auth.models import User

class Machine(models.Model):
    user = models.ManyToManyField(settings.AUTH_USER_MODEL, blank=True)
    name = models.CharField(max_length=40, unique=True)
    apitoken = models.CharField(max_length=32)
