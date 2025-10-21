# Makefile with mode switching and dependency tracking

CC       := gcc
MODE     ?= debug

ifeq ($(MODE),debug)
    CFLAGS := -std=c11 -g -O0 -Wall -Wextra -Iinclude -DDEBUG
else ifeq ($(MODE),release)
    CFLAGS := -std=c11 -O2 -Wall -Wextra -Iinclude -DNDEBUG
endif

LDFLAGS  := -lm

SRCDIR   := src
INCDIR   := include
BUILDDIR := build
TARGET   := pet_adventure

SRCS  := $(wildcard $(SRCDIR)/*.c)
OBJS  := $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SRCS))
DEPS  := $(OBJS:.o=.d)

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS)
	@echo "[LD] $@"
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

-include $(DEPS)

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	@echo "[CC] $<"
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

run: $(TARGET)
	./$(TARGET)

clean:
	@echo "Cleaning..."
	rm -rf $(BUILDDIR) $(TARGET)
