# CANLab — Virtual Vehicle Network

Three Linux C processes exchange classic CAN frames through SocketCAN on a virtual CAN interface. A Python/Tk dashboard supervises the processes and displays telemetry. Runs inside Ubuntu/WSL; hardware is not required. No pip packages needed.

## Quick start (Ubuntu terminal)

```bash
sudo apt update
sudo apt install build-essential python3 python3-tk iproute2 kmod can-utils
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan
sudo ip link set vcan0 up
```

If vcan0 already exists, skip the add command. Recreate the interface after a WSL shutdown/reboot when it is absent. Your previously verified vcan0 is suitable.

Extract the entire ZIP into your OneDrive Projects folder, preserving src/ and tests/. Open the folder containing build.py. Build and test:

```bash
python3 build.py
python3 tests/test_vcan.py vcan0
python3 dashboard.py vcan0
```

Close the dashboard before running the live tests; competing publishers on the same IDs invalidate test assumptions. Keep the interface dedicated to this project. The commands intentionally use vcan interfaces, not physical CAN hardware.

## Windows PowerShell with source files in OneDrive

For the folder `C:\Users\akhil\OneDrive\Projects\CANLab`:

```powershell
wsl -d Ubuntu -- bash -lc 'cd /mnt/c/Users/akhil/OneDrive/Projects/CANLab && python3 build.py'
wsl -d Ubuntu -- bash -lc 'cd /mnt/c/Users/akhil/OneDrive/Projects/CANLab && python3 tests/test_vcan.py vcan0'
wsl -d Ubuntu -- bash -lc 'cd /mnt/c/Users/akhil/OneDrive/Projects/CANLab && python3 dashboard.py vcan0'
```

Adjust the path if extraction creates an extra directory. Use Windows Git/VS Code for commits in OneDrive, as in Project 1. Use WSL for compilation, SocketCAN and execution. WSLg/Tk must work for the dashboard.

## What to demonstrate

1. Launch: both ECUs show OK; RPM and speed change.
2. Pause engine: after more than 500 ms without a valid frame, engine shows TIMEOUT; vehicle stays OK.
3. Engine Normal: valid frames restore OK.
4. Corrupt payload: rejected count rises, then engine times out because invalid frames do not refresh its timestamp.
5. Engine Normal, then Overheat: coolant becomes 120 C and alarm activates. Normal clears it.
6. Pause vehicle: only vehicle times out. Vehicle Normal restores it.

Numbers remain last-known values during a timeout; plot lines have gaps. Sequence events count discontinuities, including repeats and restarts, not a measured total of lost frames. Commands apply to each publisher; Normal resets its pause/corruption/heat flags.

## Architecture

- `src/ecu.c`: engine and vehicle publisher modes plus a monitor mode; CAN sockets, filters, monotonic deadlines, command handling and telemetry.
- `src/protocol.c`: encoding, validation, sequence diagnostics and freshness logic.
- `dashboard.py`: starts three C processes; sends fault commands through stdin; renders JSON produced by the CAN monitor. The GUI does not bypass CAN to deliver sensor readings.
- `tests/test_protocol.c`: deterministic format, checksum, boundary, sequence and timeout tests.
- `tests/test_vcan.py`: ten live checks for publishers, faults, isolation and recovery.

The process loop polls input and publishes nominally every 100 ms. Host scheduling can delay it; no hard-real-time claim is made. Bounded command buffers and frame validation reject malformed input. Publishing sockets disable receive filters. The monitor filters the two standard data-frame IDs and rejects malformed payloads. Traffic counters are per run.

## Custom protocol (not an OEM protocol)

Both messages are standard 11-bit classic CAN data frames, DLC 5, nominal 100 ms period.

| ID | Bytes 0–1 | Byte 2 | Byte 3 | Byte 4 |
|---|---|---|---|---|
| 0x100 | RPM, unsigned little-endian, 0–8000 | Coolant Celsius, 0–150 | Rolling uint8 counter | Application XOR checksum |
| 0x200 | Speed in 0.1 km/h, unsigned little-endian, 0–2400 | Reserved, must be zero | Rolling uint8 counter | Application XOR checksum |

Checksum is XOR of ID low byte, ID high byte and payload bytes 0–3. This weak educational checksum is neither authentication nor a replacement for the hardware CAN CRC. The corruption button damages the application checksum; it does NOT inject electrical CAN errors or bus-off. All values are fabricated; this is not OBD-II, UDS, J1939 or an OEM DBC.

## Observe raw traffic

While the dashboard is running, use another Ubuntu terminal:

```bash
candump vcan0
```

For a headless demonstration, run the following in three separate terminals after building:

```bash
./build/ecu monitor vcan0
./build/ecu engine vcan0
./build/ecu vehicle vcan0
```

In a publisher terminal, type `pause`, `normal`, `corrupt`, `heat`, or `quit` then Enter. Ctrl+C stops each process.

## Limits and honest portfolio description

This is Linux userspace C with real SocketCAN API calls on virtual CAN. It does not emulate MCU execution, physical arbitration/bit timing, bus load saturation, transceivers, electrical errors or FreeRTOS. Engine and vehicle values are independent demonstration signals, not a drivetrain model. Overheat clears immediately on the next valid normal-temperature sample. An initially absent node shows WAITING; timeout applies once at least one valid frame has arrived.

Validated in the build environment: strict GCC compilation, protocol unit checks and Python syntax. This environment blocks network administration, so live vcan tests and GUI visual checks must run on your WSL computer. CI is supplied but has not been run on your GitHub account; its virtual CAN setup depends on runner kernel support.

## First improvements to implement yourself

- Add a minimum healthy-frame count before clearing a timeout.
- Add a third message, brake status, with explicit validation and tests.
- Record faults and state transitions in CSV with monotonic timestamps.
- Write a DBC file matching this custom protocol, then compare an independent decoder with the C implementation.

Read protocol.h, protocol.c, tests/test_protocol.c, then ecu.c. Understand and modify the starter before describing your contributions in an interview. Identify it as an AI-assisted learning project where appropriate.

Reference: https://docs.kernel.org/networking/can.html
