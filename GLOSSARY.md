# OBD2 Gauge

A dash-mounted gauge that asks a car's engine computer for live values through a Bluetooth adapter and draws them on a small touchscreen.

## Language

### Talking to the car

**ELM327 adapter**:
The Bluetooth OBD-II device plugged into the car, which carries the gauge's requests to the engine computer.
_Avoid_: dongle, client, OBD2 device

**PID**:
One engine value the gauge can ask the car for, such as coolant temperature.
_Avoid_: parameter, sensor

**PID code**:
The hex text that requests a PID from the car, such as `0105`.
_Avoid_: PID (when the code is meant), command

**PID index**:
A PID's fixed position in the gauge's list of PIDs, the same on every layout.
_Avoid_: PID number, pid no

**Reply**:
The text the ELM327 adapter returns for one request, up to its `>` prompt.
_Avoid_: response, message

**Non-reply**:
A reply that carries no value, such as `NO DATA` or `CAN ERROR`.
_Avoid_: error response, empty reply

**Reading**:
The value in engineering units worked out from a reply, such as 92 °C.
_Avoid_: data, value, result

### On the screen

**Gauge**:
The whole device.
_Avoid_: meter (a meter is one part of the screen)

**Layout**:
One arrangement of meters on the screen. The button steps through eight of them.
_Avoid_: page, screen

**Slot**:
A position in a layout's polling order, holding one PID. Every layout has seven, and a slot need not have a meter.
_Avoid_: cell, pidNo

**Cell**:
A position on the screen where a meter is drawn.
_Avoid_: slot

**Meter**:
The drawing of one reading in a cell, as an arc, a number or a vertical bar.
_Avoid_: gauge

**Warning value**:
The reading beyond which a PID's meter turns red.
_Avoid_: threshold, limit

### Shutting down

**PCM volts**:
The supply voltage the car's engine computer reports, which the gauge uses to tell whether the engine is running.
_Avoid_: ECU voltage, battery voltage

**Auto-off**:
The gauge putting itself to sleep because PCM volts have stayed at or below the auto-off voltage. Only the button wakes it.
_Avoid_: engine off, ECU off

**Overheat shutdown**:
The gauge putting itself to sleep because its CPU has stayed too hot. The button or a timer wakes it.
_Avoid_: thermal shutdown
