# Generated by Django 3.2.5 on 2021-07-17 22:24

from django.db import migrations, models


class Migration(migrations.Migration):

    dependencies = [
        ('machines', '0002_machine_user'),
    ]

    operations = [
        migrations.AlterField(
            model_name='machine',
            name='apitoken',
            field=models.CharField(max_length=64),
        ),
    ]