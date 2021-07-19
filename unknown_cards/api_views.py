from django.shortcuts import render
from django.http import HttpResponse
from django.contrib.auth.decorators import login_required
from rest_framework import status
from rest_framework.decorators import api_view, permission_classes
from rest_framework.response import Response
from rest_framework import permissions
from .models import UnknownCard
import json
import logging

@api_view(['POST'])
@permission_classes((permissions.AllowAny,))
def unknown_cards_list(request):
    """
    Add an unknown card.
    """
    logger = logging.getLogger("django")
    card_id = request.data.get('card_id')
    logger.info("card: %s" % card_id)
    uc = UnknownCard(card_id=card_id)
    uc.save()
    return Response(None, status=status.HTTP_200_OK)
