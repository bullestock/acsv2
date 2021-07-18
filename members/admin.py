from django.contrib import admin
from django.contrib.auth.admin import UserAdmin
from .models import Member

class CustomUserAdmin(UserAdmin):
    fieldsets = (
        *UserAdmin.fieldsets,
        (
            'Custom fields',
            {
                'fields': (
                    'fl_id',
                    'card_id'
                ),
            },
        ),
    )

admin.site.register(Member, CustomUserAdmin)
