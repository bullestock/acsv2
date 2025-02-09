from django.urls import path

from . import api_views

urlpatterns = [
    path('', api_views.machine_v4_list),
]
