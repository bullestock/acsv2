from django.db import models
from django.contrib.auth.models import AbstractUser
from django.db.models.signals import post_save
from django.dispatch import receiver
from machines.models import Machine

class Member(AbstractUser):
    fl_id = models.IntegerField(null=True)
    fl_int_id = models.IntegerField(null=True)
    card_id = models.CharField(null=True, max_length=10)
    machine = models.ManyToManyField(Machine, blank=True)

    def __str__(self):
        return self.get_full_name()
