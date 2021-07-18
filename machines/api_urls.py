from django.urls import path

from . import api_views

app_name = 'members'

urlpatterns = [
    path('', api_views.machine_list),
]
