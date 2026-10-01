# Build/flash the MSP430FR6989 firmware with the open-source toolchain.
# Requires: msp430-gcc (TI's GCC) and mspdebug.
#
#   make          # compile -> telemetry.out
#   make flash    # flash the LaunchPad over USB
#   make clean

MCU     = msp430fr6989
CC      = msp430-elf-gcc
GCC_INC = $(MSP430_GCC_INCLUDE_DIR)          # set to your msp430-gcc include dir

CFLAGS  = -mmcu=$(MCU) -O2 -Wall -g -I$(GCC_INC)
LDFLAGS = -L$(GCC_INC) -T $(MCU).ld

SRC     = main.c hal_LCD.c
OUT     = telemetry.out

$(OUT): $(SRC) hal_LCD.h
	$(CC) $(CFLAGS) $(SRC) -o $(OUT) $(LDFLAGS)

flash: $(OUT)
	mspdebug tilib "prog $(OUT)"

clean:
	rm -f $(OUT)

.PHONY: flash clean
