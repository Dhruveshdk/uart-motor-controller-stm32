# UART-Commanded DC Motor Controller

Bare-metal firmware for the STM32 Nucleo-F411RE that drives a DC motor through an L298N driver, controlled entirely by ASCII commands sent over UART.

## Overview

This project simulates a basic host-to-MCU control interface. A command like `SPEED 70`, `FORWARD`, `REVERSE`, or `STOP` is typed into a serial terminal on a PC, sent to the STM32 over UART, parsed by the firmware, and translated into PWM duty cycle and GPIO direction signals for the motor driver.

No HAL or CubeMX is used. All peripheral configuration — clocks, GPIO, timer, and USART — is done by direct register access against the addresses defined in the STM32F411 reference manual (RM0383).

## Hardware

- STM32 Nucleo-F411RE
- L298N dual H-bridge motor driver module
- A DC motor
- External motor supply (separate from the board's 3.3V/5V rails)
- USB cable (also carries UART via the onboard ST-Link, no separate UART adapter needed)

### Pin Mapping

| STM32 Pin | Function          | Connected To     |
|-----------|--------------------|------------------|
| PA0       | TIM2 CH1 (PWM)     | L298N ENA        |
| PA1       | GPIO Output        | L298N IN2        |
| PA4       | GPIO Output        | L298N IN1        |
| PA2       | USART2 TX          | ST-Link VCP      |
| PA3       | USART2 RX          | ST-Link VCP      |

## How It Works

**Clocks.** GPIOA, TIM2, and USART2 are on different buses (AHB1 and APB1), so each is enabled separately through `RCC_AHB1ENR` / `RCC_APB1ENR` before any other configuration touches them.

**PWM.** TIM2 generates a 1 kHz PWM signal on channel 1 (PA0). Timer clock is the default 16 MHz HSI (no PLL configured). PSC = 15 gives a 1 MHz tick rate, ARR = 999 gives the 1 kHz period. Duty cycle is set by writing to CCR1, calculated as `(percent * 999) / 100`. PA0 is routed to TIM2 via alternate function mode (AF1).

**Direction.** IN1/IN2 on the L298N are driven through PA4/PA1 using the BSRR register for atomic set/clear, rather than a read-modify-write on ODR.

**UART.** USART2 runs at 9600 baud (8N1), using the Nucleo's onboard ST-Link USB-UART bridge — no external adapter required. Incoming bytes are read via polling on the RXNE flag (no interrupts in this version) and assembled into a line buffer until a newline is received. Carriage return is ignored to handle terminals that send `\r\n`.

**Command parsing.** Completed lines are matched against `STOP`, `FORWARD`, and `REVERSE` with `strcmp`, and `SPEED <value>` is parsed with `sscanf`. Speed values are clamped to 0–100 before being written to CCR1. Unrecognized input gets an explicit `Unknown command` response rather than being silently dropped.

## Command Reference

| Command       | Effect                              |
|---------------|--------------------------------------|
| `SPEED <0-100>` | Sets PWM duty cycle (clamped to 100) |
| `FORWARD`     | IN1 = HIGH, IN2 = LOW                |
| `REVERSE`     | IN1 = LOW, IN2 = HIGH                |
| `STOP`        | Duty cycle set to 0 (coasts to stop) |

Commands are terminated with a newline. Example, typed into a serial terminal at 9600 baud:

```
FORWARD
SPEED 70
STOP
```

## Building and Flashing

Built and tested with STM32CubeIDE (bare-metal project, no HAL libraries linked). Flash via the onboard ST-Link.

After flashing, open a serial terminal (PuTTY, Tera Term, etc.) on the ST-Link's COM port at 9600 baud, 8N1, no flow control.

## Known Limitations

- UART reception is polled, not interrupt-driven. Acceptable here since the main loop has no other work competing for CPU time, but would need to move to RXNE-interrupt-driven reception in a system doing anything else concurrently.
- `STOP` coasts the motor to a stop rather than actively braking (which the L298N supports by driving both IN1 and IN2 high). Coasting was chosen for simplicity and is sufficient for a small, low-inertia motor.
- Runs on the internal 16 MHz HSI oscillator rather than an external crystal with PLL. Fine for the timing tolerances needed here (1 kHz PWM, 9600 baud), but not suitable if tighter clock accuracy were ever required.
- Command buffer is a fixed 32 bytes; on overflow it resets silently rather than reporting an error.

## Possible Extensions

- Closed-loop speed control using an ADC-based tachometer/encoder feedback signal instead of open-loop UART commands.
- Interrupt-driven UART reception.
- Active braking mode on `STOP`.
