from django.urls import path

from . import api_views

urlpatterns = [
    path('<str:card_id>', api_views.machine_v2_getperm),
    path('', api_views.machine_v2_list),
]
