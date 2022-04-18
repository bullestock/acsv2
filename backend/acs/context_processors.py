from django.conf import settings

def selected_settings(request):
    return {'GIT_COMMIT': settings.GIT_COMMIT,}
