# Makefile for Experiment 2
# Project name: exp2_<YOUR GROUP NO>
# Replace <YOUR GROUP NO> with your actual group number

PROJECT = exp2_01

# Compiler and flags
CC = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE = arm-none-eabi-size

# MCU specific flags
MCU = cortex-m0plus
CFLAGS = -mcpu=$(MCU) -mthumb -O2 -Wall
LDFLAGS = -mcpu=$(MCU) -mthumb -specs=nano.specs -specs=nosys.specs

# Source files
SRCS = a.c

# Object files
OBJS = $(SRCS:.c=.o)

# Target
TARGET = $(PROJECT)

all: $(TARGET).elf $(TARGET).bin $(TARGET).hex
	@echo "Build complete for $(PROJECT)"
	$(SIZE) $(TARGET).elf

$(TARGET).elf: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET).elf $(TARGET).bin $(TARGET).hex

.PHONY: all clean
