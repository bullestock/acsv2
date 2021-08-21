from django.contrib import admin
from django.contrib.auth.admin import UserAdmin
from django import forms
from .models import Member

class CustomUserAdmin(UserAdmin):
    fieldsets = (
        *UserAdmin.fieldsets,
        (
            'Custom fields',
            {
                'fields': (
                    'fl_id',
                    'card_id',
                    'machine'
                ),
            },
        ),
    )

    def get_form(self, request, obj=None, **kwargs):
        form = super(CustomUserAdmin, self).get_form(request, obj, **kwargs)
        form.base_fields['machine'].widget = forms.CheckboxSelectMultiple()
        return form

admin.site.register(Member, CustomUserAdmin)
