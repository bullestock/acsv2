from django.contrib import admin
from unknown_cards.models import UnknownCard

class UnknownCardAdmin(admin.ModelAdmin):
    pass

admin.site.register(UnknownCard, UnknownCardAdmin)

