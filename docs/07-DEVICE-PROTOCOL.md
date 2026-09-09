# Device Simulator protocol

Device traffic is separate from the User/Admin TCP session protocol. The server listens on `--device-port` (default `18082`) using JSON Lines: one compact JSON object per newline. This is a simulator protocol, not OCPP, and it has no production-grade authentication or TLS.

## Messages

The first message must be `DEVICE_HELLO` with `messageId`, `pileId`, `pileNo`, `protocolVersion: "1.0"`, and `simulatorVersion`. The server validates the existing pile and pile number, then replies `DEVICE_HELLO_ACK` with `heartbeatIntervalSeconds` (2), `ratedPowerKw`, and the database-authoritative expected status. A newer valid connection replaces the prior connection for that pile.

Thereafter the simulator sends `DEVICE_HEARTBEAT` every two seconds, `DEVICE_TELEMETRY` (`powerKw`, `voltageV`, `currentA`, `temperatureC`, `sessionEnergyKwh`, `timestamp`) and `DEVICE_STATUS`. Fault states use `TEMP_HIGH`, `OVERVOLTAGE`, or `EMERGENCY_STOP` with a code/message. Telemetry is held only in server memory; heartbeat updates the pre-existing `charging_pile.last_heartbeat_at` field.

Server commands are `DEVICE_COMMAND` with stable `commandId`, `pileId`, `command` (`START_CHARGING`, `STOP_CHARGING`, or `RESTART`) and a payload. The simulator responds with `DEVICE_ACK` containing `commandId`, `pileId`, `command`, `success`, and `message`.

## Safety and recovery

Only piles that completed HELLO during the current server process are managed. They time out after six seconds without heartbeat; this does not change untouched legacy piles at startup. Fault and loss of a managed device transition its pile to `FAULT`/`OFFLINE`; repeated reports are idempotent. Normal order state remains server authoritative, and normal start/stop controls are optional post-transaction effects so an offline simulator cannot break legacy User flows.

On a valid reconnect, the server again reads the database-authoritative pile state. A managed `OFFLINE` pile with no active order or reservation is restored to `AVAILABLE`; `FAULT` remains `FAULT` after `DEVICE_HELLO` and must be recovered through the formal Admin restart flow. Reconnect never lets a simulator-reported normal state overwrite `RESERVED`, `CHARGING`, `FAULT`, or `RESTARTING`.

`ADMIN_PILE_RESTART` remains the only Admin restart API. A managed online device receives `RESTART` and the server moves `RESTARTING → AVAILABLE` only after a successful ACK; an ACK failure or timeout ends in `FAULT`. Piles never managed this run retain the legacy QTimer restart fallback.
