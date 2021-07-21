from django.contrib import admin
from machines.models import Machine

class MachineAdmin(admin.ModelAdmin):
    list_display = ('name', 'apitoken')
admin.site.register(Machine, MachineAdmin)
