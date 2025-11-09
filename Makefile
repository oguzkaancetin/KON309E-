PROJECT_NAME := exp2_<YOUR GROUP NO>
SRCS := exp2_2025.c pin_mux.c
OBJS := $(SRCS:.c=.o)

CC := clang
CFLAGS := -std=c99 -Wall -Wextra -Werror -O0

.PHONY: all clean flash

all: $(PROJECT_NAME)

$(PROJECT_NAME): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(PROJECT_NAME) $(OBJS)

flash: $(PROJECT_NAME)
	@echo "Flash routine not implemented. Use your preferred tool with $(PROJECT_NAME)."
