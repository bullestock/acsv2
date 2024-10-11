from django.contrib.auth.models import AbstractUser
from django.core.exceptions import ValidationError
from django.db import models
from django.dispatch import receiver
from machines.models import Machine

class Member(AbstractUser):
    fl_id = models.IntegerField(null=True)
    fl_int_id = models.IntegerField(null=True)
    card_id = models.CharField(null=True, max_length=10)
    machine = models.ManyToManyField(Machine, blank=True)

    def __str__(self):
        return self.get_full_name()

    def clean(self):
        if not self.card_id:
            return
        users = Member.objects.filter(card_id=self.card_id)
        for user in users:
            if user.id != self.id:
                raise ValidationError(f'User "{user.first_name} {user.last_name}" already has card ID {self.card_id}')
