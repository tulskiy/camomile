# Simple makefile to build static library of ffmpeg's FLAC decoder for openHiFi
# teho Labs/B. A. Bryce

CC      = arm-none-eabi-gcc
LD      = arm-none-eabi-ld -v
AR      = arm-none-eabi-ar
AS      = arm-none-eabi-as
CP      = arm-none-eabi-objcopy
OD	= arm-none-eabi-objdump


CFLAGS=-mthumb -mcpu=cortex-m3 -Os -ffunction-sections -fdata-sections -MD -std=c99 -Wall -pedantic -c -DBUILD_STANDALONE -O2
 

all: 
	$(CC) $(CFLAGS) decoder.c
	$(CC) $(CFLAGS) bitstream.c 
	$(CC) $(CFLAGS) tables.c
	$(AR) -cr flaclib.a decoder.o bitstream.o tables.o
clean:
	rm -f *.o *.d


