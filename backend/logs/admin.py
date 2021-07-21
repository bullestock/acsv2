from django.contrib import admin
from logs.models import Log

class LogAdmin(admin.ModelAdmin):
    list_display = ('stamp', 'user', 'message')
    search_fields = ('message',)
admin.site.register(Log, LogAdmin)
