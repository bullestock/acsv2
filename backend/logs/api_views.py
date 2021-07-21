from django.shortcuts import render
from django.http import HttpResponse
from django.contrib.auth.decorators import login_required
from rest_framework import status
from rest_framework.decorators import api_view, permission_classes
from rest_framework.response import Response
from rest_framework import permissions
from .models import Log
from .serializers import LogSerializer
import logging

@api_view(['POST'])
@permission_classes((permissions.AllowAny,))
def log_list(request):
    """
    Create a new log entry.
    """
    logger = logging.getLogger("django")
    logdata = request.data.get('log')
    logger.info("log: %s" % logdata)
    serializer = LogSerializer(data=logdata)
    if serializer.is_valid():
        serializer.save()
        return Response(serializer.data, status=status.HTTP_201_CREATED)
    return Response(serializer.errors, status=status.HTTP_400_BAD_REQUEST)
