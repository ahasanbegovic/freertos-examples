# FreeRTOS Examples
A collection of problems solved using [FreeRTOS](https://www.freertos.org/). Developed on the [EVK1100 board](https://www.microchip.com/webdoc/evk1100/pr01.html), designed to showcase the basics of Embedded C and FreeRTOS. Developed using [Atmel Studio 7.0.0](https://www.microchip.com/mplab/avr-support/atmel-studio-7). The board is programmed using the [AVR Dragon JTAG programmer](https://www.microchip.com/DevelopmentTools/ProductDetails/PartNO/ATAVRDRAGON).

## 1. Blink

A program that blinks the LEDs on the EVK1100 board at different intervals, LED1 = 1s, LED2 = 2s etc. Pressing a button (1-3) should make the corresponding LED (1-3) stay lit for 10 seconds while the others keep blinking. The tasks send outputs through USART. The program is designed to introduce deadline misses.

## 2. Context switching

A program that measures the time it takes for context switching between different tasks. The measurements are displayed through USART.

## 3. Semaphores

A program that displays values from three sensors (light, potentiometer and temperature) on the LCD and serial port. The values read from the sensors are communicated through message queues using one producer/consumer setup for each sensor.
