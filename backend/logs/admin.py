from django.contrib import admin
from logs.models import Log

class LogAdmin(admin.ModelAdmin):
    list_display = ('stamp', 'machine', 'user_id', 'message')
    search_fields = ('message',)
admin.site.register(Log, LogAdmin)
