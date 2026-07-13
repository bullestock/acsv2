from django.urls import path

from . import api_views

urlpatterns = [
    path('', api_views.log_list),
    path('delegate', api_views.log_delegate),
]
