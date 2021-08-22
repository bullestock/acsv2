from django.contrib import admin
from django.contrib.auth.admin import UserAdmin
from django import forms
from .models import Member

class CustomUserAdmin(UserAdmin):
    def get_fieldsets(self, request, obj=None):
        if not obj:
            return self.add_fieldsets

        if request.user.is_superuser:
            perm_fields = ('is_active', 'is_staff', 'is_superuser',
                           'groups', 'user_permissions')
        else:
            perm_fields = ('is_active', 'is_staff')

        return [ (None, {'fields': ('username', 'password')}),
                 ('Personal info', {'fields': ('first_name', 'last_name', 'email')}),
                 ('Permissions', {'fields': perm_fields}),
                 ('Custom fields', {'fields': ('fl_id', 'card_id', 'machine')})
                ]

    def get_form(self, request, obj=None, **kwargs):
        form = super(CustomUserAdmin, self).get_form(request, obj, **kwargs)
        form.base_fields['machine'].widget = forms.CheckboxSelectMultiple()
        return form

admin.site.register(Member, CustomUserAdmin)
