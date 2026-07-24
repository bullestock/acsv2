# MQTT Bidirectional Bridge

A Python script that bridges MQTT messages between a local unencrypted broker and a remote TLS-secured broker with basic authentication.

## Features

- **Bidirectional message forwarding**: Messages from either broker are forwarded to the other
- **TLS/SSL support**: Secure connection to remote broker with certificate verification
- **Basic authentication**: Username and password support for remote broker
- **Topic filtering**: Bridge specific topics or all topics
- **Configurable**: Command-line arguments or environment variables
- **Logging**: Comprehensive logging with debug mode
- **Graceful shutdown**: Proper cleanup on SIGTERM/SIGINT

## Installation

```bash
uv sync
```

## Usage

### Basic Usage

```bash
python3 mqtt_bridge.py \
  --remote-username your_username \
  --remote-password your_password
```

### With Custom Hosts/Ports

```bash
python3 mqtt_bridge.py \
  --local-host 192.168.1.100 \
  --local-port 1883 \
  --remote-host mqtt.hal9k.dk \
  --remote-port 8883 \
  --remote-username your_username \
  --remote-password your_password
```

### Using Environment Variables

```bash
export MQTT_REMOTE_USERNAME="your_username"
export MQTT_REMOTE_PASSWORD="your_password"
export MQTT_LOCAL_HOST="127.0.0.1"
export MQTT_REMOTE_HOST="mqtt.hal9k.dk"

python3 mqtt_bridge.py
```

### With Verbose Logging

```bash
python3 mqtt_bridge.py --verbose \
  --remote-username your_username \
  --remote-password your_password
```

### Topic Filtering

Bridge only specific topics:

```bash
python3 mqtt_bridge.py \
  --topic-filter "home/+/temperature" \
  --remote-username your_username \
  --remote-password your_password
```

### With Custom CA Certificate

If your remote broker uses a self-signed certificate:

```bash
python3 mqtt_bridge.py \
  --remote-ca-cert /path/to/ca.crt \
  --remote-username your_username \
  --remote-password your_password
```

## Configuration

### Command-line Arguments

- `--local-host`: Local MQTT broker host (default: 127.0.0.1)
- `--local-port`: Local MQTT broker port (default: 1883)
- `--remote-host`: Remote MQTT broker host (default: mqtt.hal9k.dk)
- `--remote-port`: Remote MQTT broker port (default: 8883)
- `--remote-username`: Remote broker username
- `--remote-password`: Remote broker password
- `--remote-ca-cert`: Path to CA certificate for remote broker verification
- `--topic-filter`: MQTT topic filter pattern (default: "#" for all topics)
- `-v, --verbose`: Enable debug logging

### Environment Variables

- `MQTT_LOCAL_HOST`: Local broker host
- `MQTT_LOCAL_PORT`: Local broker port
- `MQTT_REMOTE_HOST`: Remote broker host
- `MQTT_REMOTE_PORT`: Remote broker port
- `MQTT_REMOTE_USERNAME`: Remote broker username
- `MQTT_REMOTE_PASSWORD`: Remote broker password
- `MQTT_REMOTE_CA_CERT`: Path to CA certificate

## How It Works

1. **Local Connection**: Connects to an unencrypted local MQTT broker (port 1883)
2. **Remote Connection**: Connects to a TLS-secured remote MQTT broker with basic auth
3. **Message Forwarding**: 
   - Messages received on the local broker are forwarded to the remote broker
   - Messages received on the remote broker are forwarded to the local broker
4. **Reconnection**: Both clients will automatically attempt to reconnect if the connection is lost
5. **Logging**: All connection events and message transfers are logged

## Logging Output

- **INFO**: Connection/disconnection events, subscription status
- **DEBUG** (with -v flag): Detailed message transfer information
- **WARNING**: Unexpected disconnections, dropped messages
- **ERROR**: Connection failures, authentication errors

## Example Output

```
2026-07-24 12:21:40,000 - __main__ - INFO - Starting MQTT bridge...
2026-07-24 12:21:40,001 - __main__ - INFO - Connecting to local broker at 127.0.0.1:1883...
2026-07-24 12:21:40,002 - __main__ - INFO - Connecting to remote broker at mqtt.hal9k.dk:8883...
2026-07-24 12:21:40,500 - __main__ - INFO - Local broker: Connected successfully
2026-07-24 12:21:40,501 - __main__ - INFO - Local broker: Subscribed to #
2026-07-24 12:21:40,750 - __main__ - INFO - Remote broker: Connected successfully
2026-07-24 12:21:40,751 - __main__ - INFO - Remote broker: Subscribed to #
```

## Troubleshooting

### Connection Refused
- Ensure local broker is running on port 1883
- Verify remote broker address and port are correct

### Authentication Failed
- Check username and password are correct
- Verify credentials are properly set via environment variables or command line

### TLS/SSL Error
- Ensure the remote broker certificate is valid
- If using self-signed certificates, provide the CA certificate with `--remote-ca-cert`
- Check certificate validity dates

### Messages Not Being Forwarded
- Verify both brokers are connected (check logging output)
- Check topic filter matches your topics
- Look for log messages about dropped messages

## Running as a Service

### Systemd Service File

Create `/etc/systemd/system/mqtt-bridge.service`:

```ini
[Unit]
Description=MQTT Bidirectional Bridge
After=network.target

[Service]
Type=simple
User=mqtt
WorkingDirectory=/home/mqtt/mqtt-bridge
Environment="MQTT_REMOTE_USERNAME=your_username"
Environment="MQTT_REMOTE_PASSWORD=your_password"
ExecStart=/usr/bin/python3 /home/mqtt/mqtt-bridge/mqtt_bridge.py --verbose
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
```

Then:
```bash
sudo systemctl enable mqtt-bridge
sudo systemctl start mqtt-bridge
sudo systemctl status mqtt-bridge
```

## License

MIT
