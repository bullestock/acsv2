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

    # @receiver(pre_save)
    # def pre_save_handler(sender, instance, *args, **kwargs):
    #     if instance.card_id is not None:
    #         users = Member.objects.filter(card_id=instance.card_id)
    #         for user in users:
    #             if user.id != instance.id:
    #                 raise Exception(f'User "{user.first_name} {user.last_name}" already has card ID {instance.card_id}')
