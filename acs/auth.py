from machines.models import Machine
from members.models import Member
from rest_framework import authentication
from rest_framework import exceptions
import logging

class BodyTokenAuthentication(authentication.BaseAuthentication):
    def authenticate(self, request):
        logger = logging.getLogger("django")
        token = request.data.get('api_token')
        logger.info("API token: %s" % token)
        if not token:
            return None

        try:
            token = Machine.objects.get(apitoken=token)
        except Machine.DoesNotExist:
            raise exceptions.AuthenticationFailed('No such token')
        user = Member.objects.get(username='admin')
        return (user, None)
