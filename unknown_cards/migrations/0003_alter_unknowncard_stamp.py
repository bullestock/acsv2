# Generated by Django 3.2.5 on 2021-07-19 15:50

from django.db import migrations, models


class Migration(migrations.Migration):

    dependencies = [
        ('unknown_cards', '0002_rename_unknowncards_unknowncard'),
    ]

    operations = [
        migrations.AlterField(
            model_name='unknowncard',
            name='stamp',
            field=models.DateTimeField(auto_now_add=True, verbose_name='timestamp'),
        ),
    ]