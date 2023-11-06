from django.shortcuts import render
from django.http import HttpResponse
from django.contrib.auth.decorators import login_required
from rest_framework import status
from rest_framework.decorators import api_view, permission_classes
from rest_framework.response import Response
from rest_framework import permissions
from members.models import Member
from .models import Machine
from datetime import datetime
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
    logger.info(f"{datetime.now()} machine_list: card: %s" % card_id)
    try:
        user = Member.objects.get(card_id=card_id)
    except Member.DoesNotExist:
        res = {
	    'allowed': False,
            'error': 'Unknown card'
        }
        return Response(res, status=status.HTTP_404_NOT_FOUND)
    if not user.is_active:
        res = {
	    'allowed': False,
            'error': 'Inactive user'
        }
        return Response(res, status=status.HTTP_404_NOT_FOUND)
    u_id = user.id
    u_int_id = user.fl_int_id
    logger.info(f"{datetime.now()} machine_list: user: %d" % u_id)
    m_id = Machine.get_current_id()
    logger.info(f"{datetime.now()} machine_list: machine: %s" % m_id)
    found = user.machine.all().filter(id=m_id).exists()
    res = {
	'allowed': True if found > 0 else False,
	'id': u_id,
	'int_id': u_int_id,
	'name': user.first_name + ' ' + user.last_name
    }
    return Response(res, status=status.HTTP_200_OK)

@api_view(['GET'])
@permission_classes((permissions.AllowAny,))
def machine_v2_getperm(request, card_id):
    """
    Get a permission entry.
    """
    logger = logging.getLogger("django")
    logger.info(f"{datetime.now()} machine_v2_getperm: card: %s" % card_id)
    try:
        user = Member.objects.get(card_id=card_id)
    except Member.DoesNotExist:
        res = {
	    'allowed': False,
            'error': 'Unknown card'
        }
        return Response(res, status=status.HTTP_404_NOT_FOUND)
    if not user.is_active:
        res = {
	    'allowed': False,
            'error': 'Inactive user'
        }
        return Response(res, status=status.HTTP_404_NOT_FOUND)
    u_id = user.id
    u_int_id = user.fl_int_id
    logger.info(f"{datetime.now()} machine_v2_getperm: user: %d" % u_id)
    m_id = Machine.get_current_id()
    logger.info(f"{datetime.now()} machine_v2_getperm: machine: %s" % m_id)
    found = user.machine.all().filter(id=m_id).exists()
    res = {
	'allowed': True if found > 0 else False,
	'id': u_id,
	'int_id': u_int_id,
	'name': user.first_name + ' ' + user.last_name
    }
    return Response(res, status=status.HTTP_200_OK)

@api_view(['GET'])
@permission_classes((permissions.AllowAny,))
def machine_v2_list(request_id):
    """
    Get all permission entries.
    """
    logger = logging.getLogger("django")
    m_id = Machine.get_current_id()
    res = []
    nof_users = 0
    for user in Member.objects.all():
        nof_users += 1
        if not user.is_active:
            continue
        machines = user.machine.all()
        found = False
        for m in machines:
            if m.id == m_id:
                found = True
        if not found:
            continue
        ures = {
            'id': user.id,
            'int_id': user.fl_int_id,
            'card_id': user.card_id,
	    'name': user.first_name + ' ' + user.last_name,
        }
        res.append(ures)
    return Response(res, status=status.HTTP_200_OK)
