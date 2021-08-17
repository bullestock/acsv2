from django.shortcuts import render
from django.http import HttpResponse
from django.contrib.auth.decorators import login_required
from rest_framework import status
from rest_framework.decorators import api_view
from rest_framework.response import Response
from .models import Log
from django.views.decorators.csrf import csrf_exempt

@login_required
def index(request):
    return HttpResponse("Hello, world. You're at the logs index.")
