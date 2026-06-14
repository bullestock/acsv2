#!/usr/bin/env python3
"""
MQTT logger: subscribes to a topic and writes timestamped messages to a
rotating log file (rotated hourly, keeping 30 days of backups).
"""

import argparse
import logging
import os
import sys
from logging.handlers import TimedRotatingFileHandler

import paho.mqtt.client as mqtt

LOG_DIR = os.environ.get("LOG_DIR", "/opt/service/logs")
LOG_FILE = os.path.join(LOG_DIR, "acs")

DEFAULT_HOST = "mqtt.hal9k.dk"
DEFAULT_PORT = 8883
DEFAULT_TOPIC = "hal9k/acs/log/#"
DEFAULT_BACKUP_COUNT = 30  # keep 30 daily log files


def build_logger(log_file: str, backup_count: int) -> logging.Logger:
    os.makedirs(os.path.dirname(log_file), exist_ok=True)

    handler = TimedRotatingFileHandler(
        log_file,
        when="h",
        interval=1,
        backupCount=backup_count,
        utc=True,
    )
    # include hour in suffix so hourly rotations don't overwrite each other
    handler.suffix = "%Y-%m-%d_%H"
    handler.setFormatter(logging.Formatter("%(asctime)s|%(message)s", datefmt="%Y-%m-%dT%H:%M:%S"))

    logger = logging.getLogger("mqtt_logger")
    logger.setLevel(logging.INFO)
    logger.addHandler(handler)

    # Also echo to stdout so Docker logs work
    stdout_handler = logging.StreamHandler(sys.stdout)
    stdout_handler.setFormatter(logging.Formatter("LOG: %(asctime)s %(message)s", datefmt="%Y-%m-%dT%H:%M:%S"))
    logger.addHandler(stdout_handler)

    return logger


def on_connect(client, userdata, flags, reason_code, properties=None):
    logger = userdata["logger"]
    topic = userdata["topic"]
    if reason_code == 0:
        logger.info(f"[mqtt_logger] connected to broker, subscribing to {topic!r}")
        client.subscribe(topic)
    else:
        logger.error(f"[mqtt_logger] connection failed, reason_code={reason_code}")


def on_disconnect(client, userdata, flags, reason_code, properties=None):
    userdata["logger"].warning(f"[mqtt_logger] disconnected, reason_code={reason_code}")


def on_message(client, userdata, msg):
    userdata["logger"].info(f"{msg.topic}|{msg.payload.decode('utf-8', errors='replace')}")


def main():
    parser = argparse.ArgumentParser(description="MQTT → rotating log file")
    parser.add_argument("--host", default=os.environ.get("MQTT_HOST", DEFAULT_HOST))
    parser.add_argument("--port", type=int, default=int(os.environ.get("MQTT_PORT", DEFAULT_PORT)))
    parser.add_argument("--topic", default=os.environ.get("MQTT_TOPIC", DEFAULT_TOPIC))
    parser.add_argument("--log-file", default=os.environ.get("LOG_FILE", LOG_FILE))
    parser.add_argument("--backup-count", type=int, default=DEFAULT_BACKUP_COUNT,
                        help="Number of daily log files to keep (default: 30)")
    parser.add_argument("--tls", action=argparse.BooleanOptionalAction,
                        default=os.environ.get("MQTT_TLS", "true").lower() != "false",
                        help="Enable TLS (default: true)")
    args = parser.parse_args()

    logger = build_logger(args.log_file, args.backup_count)
    logger.info(f"[mqtt_logger] starting — broker={args.host}:{args.port} topic={args.topic!r} tls={args.tls}")

    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    client.user_data_set({"logger": logger, "topic": args.topic})
    client.on_connect = on_connect
    client.on_disconnect = on_disconnect
    client.on_message = on_message

    if args.tls:
        client.tls_set()

    client.connect(args.host, args.port, keepalive=60)
    client.loop_forever(retry_first_connection=True)


if __name__ == "__main__":
    main()
