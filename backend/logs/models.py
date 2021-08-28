from django.conf import settings
from django.db import models
from django.contrib.auth.models import User
from machines.models import Machine

class Log(models.Model):
    machine = models.ForeignKey(Machine, on_delete=models.CASCADE)
    user_id = models.ForeignKey(settings.AUTH_USER_MODEL, on_delete=models.CASCADE, null = True)
    message = models.CharField(max_length=200)
    stamp = models.DateTimeField('timestamp', auto_now_add=True)

    def has_add_permission(self, request):
        return False

    def has_change_permission(self, request, obj=None):
        return False

    def has_delete_permission(self, request, obj=None):
        return False
