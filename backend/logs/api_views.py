from django.shortcuts import render
from django.http import HttpResponse
from django.contrib.auth.decorators import login_required
from rest_framework import status
from rest_framework.decorators import api_view, permission_classes
from rest_framework.response import Response
from rest_framework import permissions
from .models import Log
from .models import Machine
from members.models import Member
import logging

@api_view(['POST'])
@permission_classes((permissions.AllowAny,))
def log_list(request):
    """
    Create a new log entry.
    """
    logger = logging.getLogger("django")
    logdata = request.data.get('log')
    logger.info("log view: %s" % logdata)
    l = Log(machine=Machine.objects.get(id=Machine.get_current_id()),
            user_id=Member.objects.get(id=logdata['user_id']),
            message=logdata['message'])
    l.save()
    return Response(None, status=status.HTTP_200_OK)
