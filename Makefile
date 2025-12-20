#############################################################################
# Makefile for Jaguar Physics Demo
#############################################################################

# Check for Jaguar SDK environment
ifdef JAGSDK
include $(JAGSDK)/tools/build/jagdefs.mk
endif

#############################################################################
# Build Configuration
#############################################################################

PROG = jag_physics.cof

# Source Files
SRC_C_COMMON = src/demo_main.c src/jag_physics.c src/demo_bitmap.c src/jag_gpu.c src/libgcc.c
SRC_C_JAGUAR = src/jag_stubs.c src/jag_main.c
SRC_C_PC = src/string.c src/stdlib.c src/stdio.c
SRC_C = $(SRC_C_COMMON) $(SRC_C_JAGUAR)
SRC_S = src/jag_startup.s

# PC Build Configuration
CC_PC = gcc
CFLAGS_PC = -Wall -O2 -Isrc

# OS Detection for Clean
ifeq ($(OS),Windows_NT)
	RM_CMD = del /Q /F
	EXT = .exe
else
	RM_CMD = rm -f
	EXT =
endif

#############################################################################
# Targets
#############################################################################

.PHONY: all jaguar ascii clean

# Default Target (Jaguar)
all: jaguar

# Only define SDK rules if SDK is present to avoid errors on PC
ifdef JAGSDK

# Jaguar Compiler Flags
CFLAGS += -std=c99 -mshort -Wall -fno-builtin -nostdinc -Isrc/include -Isrc -DJAGUAR
LINKFLAGS +=

# Objects
OBJS = $(SRC_S:.s=.o) $(SRC_C:.c=.o)

# Linker Rule (relies on jagrules.mk for standard libs/startup)
# We manually specify the link command here if needed, or rely on rules.
# The SDK example uses $(LINK) and manually constructs the object list including CRT0.
# Let's follow the example closer.

OBJS_LINK = $(OBJS)

# Jaguar Build Rule
jaguar: $(PROG)

$(PROG): $(OBJS)
	$(LINK) $(LINKFLAGS) -o $@ $(OBJS_LINK)

include $(JAGSDK)/tools/build/jagrules.mk

else

# Fallback/Help if SDK not found
jaguar:
	@echo "Error: JAGSDK environment variable not set or SDK not found."
	@echo "Please run this target inside the Devcontainer or set up the Jaguar SDK."
	@exit 1

endif

# PC/ASCII Build Rule
ascii: jag_physics_pc$(EXT)

jag_physics_pc$(EXT): $(SRC_C_COMMON) $(SRC_C_PC)
	$(CC_PC) $(CFLAGS_PC) -o $@ $^

clean:
	$(RM_CMD) jag_physics_pc$(EXT) src/*.o *.o *.cof *.sym *.map
