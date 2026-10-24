# Validation

- GCC C11 compilation with -Wall -Wextra -Werror -pedantic: passed.
- Protocol unit checks: passed.
- Python syntax checks: passed.
- Non-vcan argument rejection: passed.
- Live SocketCAN integration: not run; this environment denies network administration. Run python3 tests/test_vcan.py vcan0 locally.
- Tk dashboard visual check: not performed in this headless environment.
