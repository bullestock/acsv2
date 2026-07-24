#!/usr/bin/env python3
"""
MQTT Broker and Bidirectional Bridge

A Python script that:
1. Acts as an MQTT broker accepting connections on localhost:1883 (unencrypted)
2. Bridges messages bidirectionally to/from a remote TLS broker (mqtt.hal9k.dk:8883)

This script uses paho-mqtt for efficient handling of both the local broker
and remote bridge connections.
"""

import logging
import os
import signal
import ssl
import sys
import time
from typing import Optional

import paho.mqtt.client as mqtt

# Configuration
LOCAL_BROKER_HOST = "127.0.0.1"
LOCAL_BROKER_PORT = 1883

REMOTE_BROKER_HOST = "mqtt.hal9k.dk"
REMOTE_BROKER_PORT = 8883
REMOTE_BROKER_USERNAME = ""
REMOTE_BROKER_PASSWORD = ""
REMOTE_BROKER_TLS_CA_CERT = None

TOPIC_FILTER = "#"  # Bridge all topics

# Set up logging
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s - %(name)s - %(levelname)s - %(message)s",
)
logger = logging.getLogger(__name__)


class MQTTBrokerBridge:
    """MQTT Broker bridge connecting local and remote brokers bidirectionally."""

    def __init__(
        self,
        local_host: str = LOCAL_BROKER_HOST,
        local_port: int = LOCAL_BROKER_PORT,
        remote_host: str = REMOTE_BROKER_HOST,
        remote_port: int = REMOTE_BROKER_PORT,
        remote_username: Optional[str] = None,
        remote_password: Optional[str] = None,
        remote_ca_cert: Optional[str] = None,
        topic_filter: str = TOPIC_FILTER,
    ):
        """Initialize the MQTT broker bridge."""
        self.local_host = local_host
        self.local_port = local_port
        self.remote_host = remote_host
        self.remote_port = remote_port
        self.remote_username = remote_username
        self.remote_password = remote_password
        self.remote_ca_cert = remote_ca_cert
        self.topic_filter = topic_filter

        self.running = False
        self.local_client = None
        self.remote_client = None
        self.forward_from_local = True
        self.forward_from_remote = True

    def setup_local_client(self) -> mqtt.Client:
        """Set up the local MQTT client (acts as connection to local broker)."""
        client = mqtt.Client(
            mqtt.CallbackAPIVersion.VERSION2,
            client_id="mqtt-broker-bridge",
        )
        client.on_connect = self._on_local_connect
        client.on_disconnect = self._on_local_disconnect
        client.on_message = self._on_local_message
        client.on_publish = self._on_local_publish
        
        return client

    def setup_remote_client(self) -> mqtt.Client:
        """Set up the remote MQTT client (TLS with basic auth)."""
        client = mqtt.Client(
            mqtt.CallbackAPIVersion.VERSION2,
            client_id="mqtt-bridge-remote",
        )
        client.on_connect = self._on_remote_connect
        client.on_disconnect = self._on_remote_disconnect
        client.on_message = self._on_remote_message
        
        # Configure TLS
        if self.remote_ca_cert:
            client.tls_set(
                ca_certs=self.remote_ca_cert,
                certfile=None,
                keyfile=None,
                cert_reqs=ssl.CERT_REQUIRED,
                tls_version=ssl.PROTOCOL_TLSv1_2,
                ciphers=None,
            )
            logger.info(f"Using CA certificate: {self.remote_ca_cert}")
        else:
            client.tls_set()
            client.tls_insecure_set(True)
            logger.warning("TLS verification disabled - not recommended for production")

        # Set basic auth
        if self.remote_username and self.remote_password:
            client.username_pw_set(self.remote_username, self.remote_password)
            logger.info(f"Remote authentication: {self.remote_username}")

        return client

    def _on_local_connect(self, client, userdata, connect_flags, reason_code, properties):
        """Called when local client connects."""
        if reason_code == 0:
            logger.info("Local broker: Connected successfully")
            client.subscribe(self.topic_filter, qos=1)
            logger.info(f"Local broker: Subscribed to {self.topic_filter}")
        else:
            logger.error(f"Local broker: Connection failed with code {reason_code}")

    def _on_local_disconnect(self, client, userdata, disconnect_flags, reason_code, properties):
        """Called when local client disconnects."""
        if reason_code == 0:
            logger.info("Local broker: Disconnected cleanly")
        else:
            logger.warning(f"Local broker: Unexpected disconnection with code {reason_code}")
            # Attempt to reconnect
            if self.running:
                logger.info("Local broker: Attempting to reconnect...")
                try:
                    client.reconnect()
                except Exception as e:
                    logger.error(f"Failed to reconnect to local broker: {e}")

    def _on_local_message(self, client, userdata, msg):
        """Called when message received on local broker - forward to remote."""
        if not self.forward_from_local:
            return
            
        logger.debug(
            f"Local -> Remote: {msg.topic} "
            f"({len(msg.payload)} bytes, QoS {msg.qos}, Retain {msg.retain})"
        )
        
        if self.remote_client and self.remote_client.is_connected():
            self.forward_from_remote = False  # Prevent loop
            try:
                self.remote_client.publish(
                    msg.topic,
                    msg.payload,
                    qos=msg.qos,
                    retain=msg.retain,
                )
            finally:
                self.forward_from_remote = True
        else:
            logger.warning(f"Remote broker not connected, dropping message: {msg.topic}")

    def _on_local_publish(self, client, userdata, mid, reason_codes, properties):
        """Called when message is published to local broker."""
        logger.debug(f"Local broker: Message {mid} published")

    def _on_remote_connect(self, client, userdata, connect_flags, reason_code, properties):
        """Called when remote client connects."""
        if reason_code == 0:
            logger.info("Remote broker: Connected successfully")
            client.subscribe(self.topic_filter, qos=1)
            logger.info(f"Remote broker: Subscribed to {self.topic_filter}")
        else:
            logger.error(f"Remote broker: Connection failed with code {reason_code}")

    def _on_remote_disconnect(self, client, userdata, disconnect_flags, reason_code, properties):
        """Called when remote client disconnects."""
        if reason_code == 0:
            logger.info("Remote broker: Disconnected cleanly")
        else:
            logger.warning(f"Remote broker: Unexpected disconnection with code {reason_code}")
            # Attempt to reconnect
            if self.running:
                logger.info("Remote broker: Attempting to reconnect...")
                try:
                    client.reconnect()
                except Exception as e:
                    logger.error(f"Failed to reconnect to remote broker: {e}")

    def _on_remote_message(self, client, userdata, msg):
        """Called when message received on remote broker - forward to local."""
        if not self.forward_from_remote:
            return
            
        logger.debug(
            f"Remote -> Local: {msg.topic} "
            f"({len(msg.payload)} bytes, QoS {msg.qos}, Retain {msg.retain})"
        )
        
        if self.local_client and self.local_client.is_connected():
            self.forward_from_local = False  # Prevent loop
            try:
                self.local_client.publish(
                    msg.topic,
                    msg.payload,
                    qos=msg.qos,
                    retain=msg.retain,
                )
            finally:
                self.forward_from_local = True
        else:
            logger.warning(f"Local broker not connected, dropping message: {msg.topic}")

    def start(self):
        """Start the broker bridge."""
        logger.info("=" * 60)
        logger.info("Starting MQTT Broker Bridge")
        logger.info("=" * 60)
        logger.info(f"Local broker:  {self.local_host}:{self.local_port} (unencrypted)")
        logger.info(f"Remote broker: {self.remote_host}:{self.remote_port} (TLS)")
        logger.info(f"Topic filter:  {self.topic_filter}")
        logger.info("=" * 60)
        
        self.running = True

        try:
            # Set up clients
            self.local_client = self.setup_local_client()
            self.remote_client = self.setup_remote_client()

            # Connect to local broker
            logger.info(f"Connecting to local broker at {self.local_host}:{self.local_port}...")
            self.local_client.connect(
                self.local_host,
                self.local_port,
                keepalive=60,
            )
            self.local_client.loop_start()

            # Give local client time to connect
            time.sleep(0.5)

            # Connect to remote broker
            logger.info(f"Connecting to remote broker at {self.remote_host}:{self.remote_port}...")
            self.remote_client.connect(
                self.remote_host,
                self.remote_port,
                keepalive=60,
            )
            self.remote_client.loop_start()

            logger.info("Bridge is running. Press Ctrl+C to stop.")
            logger.info("=" * 60)

            # Keep running
            while self.running:
                time.sleep(1)

        except KeyboardInterrupt:
            logger.info("Interrupted by user")
            self.stop()
        except Exception as e:
            logger.error(f"Error in bridge: {e}", exc_info=True)
            self.stop()
            raise

    def stop(self):
        """Stop the broker bridge."""
        logger.info("=" * 60)
        logger.info("Stopping MQTT Broker Bridge...")
        self.running = False

        if self.local_client:
            try:
                self.local_client.loop_stop()
                self.local_client.disconnect()
                logger.info("Local broker: Disconnected")
            except Exception as e:
                logger.error(f"Error disconnecting from local broker: {e}")

        if self.remote_client:
            try:
                self.remote_client.loop_stop()
                self.remote_client.disconnect()
                logger.info("Remote broker: Disconnected")
            except Exception as e:
                logger.error(f"Error disconnecting from remote broker: {e}")

        logger.info("Bridge stopped")
        logger.info("=" * 60)

    def signal_handler(self, sig, frame):
        """Handle shutdown signals."""
        logger.info(f"\nReceived signal {sig}")
        self.stop()
        sys.exit(0)


def main():
    """Main entry point."""
    import argparse

    parser = argparse.ArgumentParser(
        description="MQTT Broker with bidirectional bridge to remote broker",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Basic usage (connect to remote broker with auth)
  python3 mqtt_broker_bridge.py --remote-username user --remote-password pass

  # Custom local port
  python3 mqtt_broker_bridge.py --local-port 1883 --remote-username user --remote-password pass

  # With custom CA certificate
  python3 mqtt_broker_bridge.py --remote-ca-cert /path/to/ca.crt --remote-username user --remote-password pass

  # Using environment variables
  export MQTT_REMOTE_USERNAME="user"
  export MQTT_REMOTE_PASSWORD="pass"
  python3 mqtt_broker_bridge.py
        """,
    )

    parser.add_argument(
        "--local-host",
        default=os.getenv("MQTT_LOCAL_HOST", LOCAL_BROKER_HOST),
        help="Local MQTT broker host (default: %(default)s)",
    )
    parser.add_argument(
        "--local-port",
        type=int,
        default=int(os.getenv("MQTT_LOCAL_PORT", LOCAL_BROKER_PORT)),
        help="Local MQTT broker port (default: %(default)s)",
    )
    parser.add_argument(
        "--remote-host",
        default=os.getenv("MQTT_REMOTE_HOST", REMOTE_BROKER_HOST),
        help="Remote MQTT broker host (default: %(default)s)",
    )
    parser.add_argument(
        "--remote-port",
        type=int,
        default=int(os.getenv("MQTT_REMOTE_PORT", REMOTE_BROKER_PORT)),
        help="Remote MQTT broker port (default: %(default)s)",
    )
    parser.add_argument(
        "--remote-username",
        default=os.getenv("MQTT_REMOTE_USERNAME", REMOTE_BROKER_USERNAME),
        required=True,
        help="Remote broker username (can be set via MQTT_REMOTE_USERNAME env var)",
    )
    parser.add_argument(
        "--remote-password",
        default=os.getenv("MQTT_REMOTE_PASSWORD", REMOTE_BROKER_PASSWORD),
        required=True,
        help="Remote broker password (can be set via MQTT_REMOTE_PASSWORD env var)",
    )
    parser.add_argument(
        "--remote-ca-cert",
        default=os.getenv("MQTT_REMOTE_CA_CERT"),
        help="Path to CA certificate for remote broker (optional)",
    )
    parser.add_argument(
        "--topic-filter",
        default=TOPIC_FILTER,
        help="MQTT topic filter (default: %(default)s)",
    )
    parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        help="Enable verbose (debug) logging",
    )

    args = parser.parse_args()

    if args.verbose:
        logging.getLogger().setLevel(logging.DEBUG)
        logger.debug("Debug logging enabled")

    bridge = MQTTBrokerBridge(
        local_host=args.local_host,
        local_port=args.local_port,
        remote_host=args.remote_host,
        remote_port=args.remote_port,
        remote_username=args.remote_username if args.remote_username else None,
        remote_password=args.remote_password if args.remote_password else None,
        remote_ca_cert=args.remote_ca_cert,
        topic_filter=args.topic_filter,
    )

    # Set up signal handlers
    signal.signal(signal.SIGINT, bridge.signal_handler)
    signal.signal(signal.SIGTERM, bridge.signal_handler)

    bridge.start()


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        logger.info("Bridge terminated")
    except Exception as e:
        logger.error(f"Fatal error: {e}", exc_info=True)
        sys.exit(1)
