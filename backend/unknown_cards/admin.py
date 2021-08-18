from django.contrib import admin
from unknown_cards.models import UnknownCard

class UnknownCardAdmin(admin.ModelAdmin):
    list_display = ('stamp', 'card_id')

    def has_add_permission(self, request):
        return False

admin.site.register(UnknownCard, UnknownCardAdmin)
