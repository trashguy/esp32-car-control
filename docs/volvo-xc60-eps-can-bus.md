# Volvo XC60 Electric Power Steering Pump CAN Bus Protocol

## Pump Identification

- **Vehicle:** 2014-2017 Volvo XC60
- **Part Number:** 458864
- **Type:** Hydraulic Electric Power Steering (EHPS) pump with internal VFD

## Compatible Models

This protocol applies to Volvo EHPS pumps from:
- C30, C70, S40, V50 (2005-2013)
- S60, V60, V70, XC60, XC70, S80 (2010-2015)

## Electrical Requirements

- **Voltage:** 12-14V DC
- **Current:** 6-80A (use 80A fuse minimum)
- **Wiring:** +12V ignition, CAN H, CAN L, Ground

## CAN Bus Configuration

- **Baud Rate:** 500 kbps
- **Frame Type:** Extended (29-bit IDs)

## CAN Message IDs

| ID (hex) | Direction | Rate | Purpose |
|----------|-----------|------|---------|
| `0x1B200002` | Pump → Controller | ~11.84 Hz | Pump status/alive message |
| `0x1AE0092C` | Controller → Pump | ~2.38 Hz | Keep-alive heartbeat |
| `0x02104136` | Controller → Pump | ~71.4 Hz | Vehicle speed / effort control |

## Message Details

### Pump Status (`0x1B200002`)

Sent by pump when powered and in CAN mode. Example frame:
```
0x1B200002 [8]: 00 00 00 00 00 00 0F 71
```

### Keep-Alive (`0x1AE0092C`)

Must be sent periodically to keep pump in CAN-controlled mode. Send at approximately 1/30 the rate of the speed control message.

### Speed Control (`0x02104136`)

Controls pump motor speed/effort based on vehicle speed.

- **Bytes 6-7:** 16-bit big-endian unsigned vehicle speed value
- **Logic is inverted:** Small values = pump runs fast (high assist), Large values = pump runs slow (low assist)
- Other bytes can remain static

## Failsafe Mode (No CAN)

The pump will operate in failsafe mode without CAN bus communication:
- Connect only power (+12V, ignition, ground)
- Pump runs at fixed default speed
- Variable speed-sensitive assist is lost
- Suitable for applications where constant assist is acceptable

## Implementation Notes

1. Wait for pump's alive message (`0x1B200002`) before sending control commands
2. Route power steering power outside PDM systems - use separate fuse/relay due to flyback generation risk
3. The CAN bus primarily controls current limit on motor (pressure limit) rather than steering angle

## References

- [rusefi.com Forum Thread](https://rusefi.com/forum/viewtopic.php?t=2329) - Primary reverse engineering source
- [MaxxECU Documentation](https://www.maxxecu.com/webhelp/can_peripheral_control_volvo_powersteering.html) - Commercial ECU implementation
- [Reform Motorsports Controller](https://reformmotorsports.com/product/volvo-ehps-micro/) - Standalone controller option
- [DIY Electric Car Forums](https://www.diyelectriccar.com/threads/can-bus-control-of-volvo-steering-pump.82580/) - Community discussion
