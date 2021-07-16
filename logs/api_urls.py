from django.urls import path

from . import api_views

app_name = 'logs'

urlpatterns = [
    path('', api_views.log_list),
]
