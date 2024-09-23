# --
# Copyright (c) 2016, Lukasz Marcin Podkalicki <lpodkalicki@gmail.com>
# --

MCU=attiny13

# TJT - conflicts with Biscotti
#F_CPU=1200000

#FUSE_L=0x6A
FUSE_L=0x75
FUSE_H=0xFF
CC=avr-gcc
LD=avr-ld
OBJCOPY=avr-objcopy
SIZE=avr-size
AVRDUDE=avrdude

#CFLAGS=-std=c99 -Wall -g -Os -mmcu=${MCU} -DF_CPU=${F_CPU} -I. -c
#CFLAGS=-std=c99 -Wall -g -Os -mmcu=${MCU} -DF_CPU=${F_CPU} -I.
#CFLAGS=-Wall -g -Os -mmcu=${MCU} -DF_CPU=${F_CPU} -I.
CFLAGS=-Wall -g -Os -mmcu=${MCU} -I.

TARGET=biscotti

SRCS = biscotti.c

all:
#	${CC} ${CFLAGS} -o ${TARGET}.o ${SRCS}
	${CC} ${CFLAGS} -o ${TARGET}.elf ${SRCS}
#	${LD} -o ${TARGET}.elf ${TARGET}.o
#	${OBJCOPY} -j .text -j .data -O ihex ${TARGET}.o ${TARGET}.hex
	${OBJCOPY} -O ihex ${TARGET}.elf ${TARGET}.hex
	${SIZE} -C --mcu=${MCU} ${TARGET}.elf

flash:
#	${AVRDUDE} -p ${MCU} -c usbasp -B10 -B10 -U flash:w:${TARGET}.hex:i -F -P usb
#   ${AVRDUDE} -p ${MCU} -c usbasp -B10 -U flash:w:${TARGET}.hex:i -F -P usb
	${AVRDUDE} -p ${MCU} -c usbasp -B10 -U flash:w:${TARGET}.hex

dump: $(TARGET).elf
	avr-objdump -d $(TARGET).elf >$(TARGET).dump

# untested -- some people report that it is essential to erase a chip in a
# flashlight before you can program it.
erase:
	avrdude -p t13 -c usbasp -u -e

fuse:
	$(AVRDUDE) -p ${MCU} -c usbasp -B10 -U hfuse:w:${FUSE_H}:m -U lfuse:w:${FUSE_L}:m

# Try to read a fuse
# This outputs intel hex, which is dumb
#rfuseI:
#	$(AVRDUDE) -p ${MCU} -c usbasp -B10 -U hfuse:r:-:i -U lfuse:r:-:i

# Try to read a fuse (another output mode)
# This outputs just the hex value for the fuse.  Much better.
rfuse:
	$(AVRDUDE) -p ${MCU} -c usbasp -B10 -U hfuse:r:-:h -U lfuse:r:-:h

clean:
	rm -f *.c~ *.h~ *.o *.elf *.hex *.dump
