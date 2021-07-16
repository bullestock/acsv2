from rest_framework import serializers
from members.models import Member
from .models import Log

class LogSerializer(serializers.Serializer):
    user = serializers.PrimaryKeyRelatedField(source='user_id', queryset=Member.objects.all())
    message = serializers.CharField(max_length=200)

    def create(self, validated_data):
        """
        Create and return a new `Log` instance, given the validated data.
        """
        return Log.objects.create(**validated_data)
