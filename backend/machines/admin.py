from django.contrib import admin
from machines.models import Machine

class MachineAdmin(admin.ModelAdmin):
    list_display = ('name',)

admin.site.register(Machine, MachineAdmin)
