from django.shortcuts import render
from django.http import HttpResponse
from django.contrib.auth.decorators import login_required
from rest_framework import status
from rest_framework.decorators import api_view, permission_classes
from rest_framework.response import Response
from rest_framework import permissions
from members.models import Member
from .models import Machine
import json
import logging

@api_view(['POST'])
@permission_classes((permissions.AllowAny,))
def machine_list(request):
    """
    Get a permission entry.
    """
    logger = logging.getLogger("django")
    card_id = request.data.get('card_id')
    logger.info("card: %s" % card_id)
    try:
        user = Member.objects.get(card_id=card_id)
    except Member.DoesNotExist:
        return Response('Unknown card', status=status.HTTP_404_NOT_FOUND)
    u_id = user.id
    logger.info("user: %d" % u_id)
    m_id = Machine.get_current_id()
    logger.info("machine: %s" % m_id)
    found = Machine.user.through.objects.all().filter(machine_id__exact=m_id, member_id__exact=u_id).count()
    res = {
	'allowed': True if found > 0 else False,
	'id': u_id,
	'name': user.first_name + ' ' + user.last_name
    }
    return Response(res, status=status.HTTP_200_OK)
