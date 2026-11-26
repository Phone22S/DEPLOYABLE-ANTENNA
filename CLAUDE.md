# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Working rules

- The user is a beginner, with one month of C/STM32 experience (as of November 2026).
- Reply in Thai, and always explain the reasoning.
- Explain the plan before editing any code.
- Never edit code outside `USER CODE BEGIN/END` blocks. If a fix requires it, explain the problem and the change needed instead of making the edit.

## What this is

Firmware for a deployable antenna released by a burn wire, on the STM32G431KBTx (STM32 HAL, 16 MHz HSI, no PLL). A heater on PA0 burns the wire, and a limit switch on PA1 reports the deployment state.

## Build and test

The repo root is an STM32CubeIDE project (`BurnWire_Test`). Build, flash and debug from CubeIDE. No command-line build script, tests or linter are committed, and `Debug/` and `Release/` are ignored. Testing is done on hardware over UART at 115200 8N1.

## Two separate bodies of code

**The root project (`Core/`, `Drivers/`, `BurnWire_Test.ioc`)** is an early GPIO-only burn-wire test. `Core/Src/main.c` never sets `deployCommand`, so as written it never turns the heater on.

**`Nucleo/`** holds standalone copies of `main.c` for newer UART firmware. They are not part of the root build, which compiles only `Core/` and `Drivers/`. They also cannot be dropped into it as is:

- `stm32g4xx_hal_conf.h` does not enable the UART or IWDG modules, and their HAL driver sources are absent.
- `stm32g4xx_hal_msp.c` has no UART pin or NVIC setup, and `stm32g4xx_it.c` has no USART interrupt handlers.

Each file is meant to replace `main.c` in a CubeMX-generated project that has those pieces. That project is not in this repo, so check with the user how they build these files.

CubeMX regeneration overwrites code outside `USER CODE BEGIN/END` blocks. Some existing code sits outside them, which does not relax the working rule above.

## Firmware variants in `Nucleo/`

Board A is the OBC (on-board computer), relaying between a PC (USART2) and Board B (USART1). Board B is the antenna controller (ANT), with USART1 linked to A.

| Stage | Board A | Board B | Protocol |
|---|---|---|---|
| Single board | none | `main c.c` | Single-byte commands in, `STATUS; ...` text out, all on USART1 |
| v1 raw | `Board A END to E.c` | `UART B.c` (`UART B.txt` is an identical copy) | Binary: `0xAA, src, dest, cmd, data` |
| v2 hex string | `HEX str A.c` | `HEX str B.c` | 8 hex characters (`src dest cmd data`) ended by CR or LF, no sync byte |

The v2 pair is recorded as tested OK in commit `d5fbd1e`.

**Burn control exists only in `main c.c`.** The v1 and v2 Board B files validate commands and send reply codes, but never drive the heater or read the limit switch. A `0x10` reply to a deploy command therefore does not mean anything was heated. Combining the v2 protocol with the control logic in `main c.c` has not been done in any file here.

### Packet protocol (v1 and v2)

- Node IDs: OBC `0x01`, ANT `0x02`, GND `0x10`.
- Commands: `0xC3` ping, `0x3C` status, `0xA5` deploy (`data` is burn time in seconds, valid from 1 to 30), `0x5A` emergency stop.
- Board B replies, carried in `data`: ping `0x02`, status `0x30` (fixed value), deploy accepted `0x10`, deploy rejected `0x13`, emergency stop `0x22`, unknown command `0xEE`. In v2, a source other than the OBC also gets `0x13`.
- Board A routing: packets from the PC for ANT are forwarded with `src` rewritten to OBC. For OBC, only ping is answered (`0x02`), and other commands get `0xEE`. Any other destination gets `0xE1`. Packets from B addressed to OBC are relayed to the PC with `dest` set to GND.
- Boot message: `cmd=0x00, data=0x01`.

A protocol change has to be made on both boards of the same stage.

### Runtime pattern

UART receive is interrupt-driven, one byte at a time. The RX callback parses the byte and sets a flag or fills a single-slot packet buffer, and the main loop does the work with blocking transmits. A packet that completes while the slot is full is dropped. Both the RX and error callbacks must re-arm `HAL_UART_Receive_IT` on the same UART, or reception stops. The error callback in `main c.c` re-arms `huart2` when USART1 fails.

## Heater safety behavior

In `main c.c`, the heater turns on only if the limit pin reads low (not yet deployed). It turns off when the pin goes high, after the 10 s timeout (`timeSET`), on emergency stop, and in `Error_Handler`. The independent watchdog (prescaler 32, reload 999, about 1 s) is refreshed once per main loop pass, so blocking work in the loop must stay well below that. Keep all of these when changing or porting the burn logic.

The limit pin has an internal pull-up in `BurnWire_Test.ioc` and `Core/Src/main.c`, but the `Nucleo/` files configure it with `GPIO_NOPULL`. Confirm the wiring before relying on either setting.

## Misleading filenames

`LOOPH+KISS+CSP.c` at the root contains a `FreeRTOSConfig.h`. It has no loopback, KISS or CSP code, and the CubeIDE project does not use FreeRTOS.
