from django.urls import path

from . import api_views

app_name = 'unknown_cards'

urlpatterns = [
    path('', api_views.unknown_cards_list),
]
