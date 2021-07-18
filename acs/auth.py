from machines.models import Machine
from members.models import Member
from rest_framework import authentication
from rest_framework import exceptions
import logging

class BodyTokenAuthentication(authentication.BaseAuthentication):
    def authenticate(self, request):
        Machine.set_current_token(None)
        Machine.set_current_id(None)
        logger = logging.getLogger("django")
        token = request.data.get('api_token')
        logger.info("API token: %s" % token)
        if not token:
            return None

        try:
            m = Machine.objects.get(apitoken=token)
            Machine.set_current_token(m.apitoken)
            Machine.set_current_id(m.id)
        except Machine.DoesNotExist:
            raise exceptions.AuthenticationFailed('No such token')
        user = Member.objects.get(username='admin')
        return (user, None)
