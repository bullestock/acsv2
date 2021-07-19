from django.urls import path

from . import api_views

urlpatterns = [
    path('', api_views.unknown_cards_list),
]
