# Generated by Django 3.2.6 on 2021-08-16 18:20

from django.conf import settings
from django.db import migrations, models
import django.db.models.deletion


class Migration(migrations.Migration):

    dependencies = [
        migrations.swappable_dependency(settings.AUTH_USER_MODEL),
        ('machines', '0003_alter_machine_apitoken'),
        ('logs', '0003_rename_text_log_message'),
    ]

    operations = [
        migrations.AddField(
            model_name='log',
            name='machine',
            field=models.ForeignKey(default=1, on_delete=django.db.models.deletion.CASCADE, to='machines.machine'),
            preserve_default=False,
        ),
        migrations.AlterField(
            model_name='log',
            name='user',
            field=models.ForeignKey(null=True, on_delete=django.db.models.deletion.CASCADE, to=settings.AUTH_USER_MODEL),
        ),
    ]
