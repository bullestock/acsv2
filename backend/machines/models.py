from django.conf import settings
from django.db import models
from django.contrib.auth.models import User

class Machine(models.Model):
    current_token = None
    current_id = None
    user = models.ManyToManyField(settings.AUTH_USER_MODEL, blank=True)
    name = models.CharField(max_length=40, unique=True)
    apitoken = models.CharField(max_length=64)

    def __str__(self):
        return self.name

    @classmethod
    def set_current_token(cls, token):
        cls.current_token = token

    @classmethod
    def get_current_token(cls):
        return cls.current_token

    @classmethod
    def set_current_id(cls, id):
        cls.current_id = id

    @classmethod
    def get_current_id(cls):
        return cls.current_id
