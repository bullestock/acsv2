from django.contrib import admin
from unknown_cards.models import UnknownCard

class UnknownCardAdmin(admin.ModelAdmin):
    list_display = ('stamp', 'card_id')
admin.site.register(UnknownCard, UnknownCardAdmin)
