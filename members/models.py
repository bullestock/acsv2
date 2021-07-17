from django.db import models
from django.contrib.auth.models import AbstractUser
from django.db.models.signals import post_save
from django.dispatch import receiver

class Member(AbstractUser):
    fl_id = models.IntegerField(null=True)
    card_id = models.CharField(null=True, max_length=10)
