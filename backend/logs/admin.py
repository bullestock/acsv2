from django.contrib import admin
from logs.models import Log

class LogAdmin(admin.ModelAdmin):
    list_display = ('stamp', 'machine', 'user_id', 'message')
    search_fields = ('message', 'user_id__first_name', 'user_id__last_name', 'machine_id__name')

    def has_add_permission(self, request):
        return False

admin.site.register(Log, LogAdmin)
