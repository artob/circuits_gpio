# Variables to override
#
# CC            C compiler
# CROSSCOMPILE	crosscompiler prefix, if any
# CFLAGS	compiler flags for compiling all C files
# ERL_CFLAGS	additional compiler flags for files using Erlang header files
# ERL_EI_INCLUDE_DIR include path to ei.h (Required for crosscompile)
# ERL_EI_LIBDIR path to libei.a (Required for crosscompile)
# LDFLAGS	linker flags for linking all binaries
# ERL_LDFLAGS	additional linker flags for projects referencing Erlang libraries

NIF=priv/gpio_nif.so
HELPER=priv/gpio_helper

TARGETS=$(NIF)

NIF_LDFLAGS = $(LDFLAGS)
HELPER_LDFLAGS = $(LDFLAGS)
TARGET_CFLAGS = $(shell src/detect_target.sh)

# Check that we're on a supported build platform
ifeq ($(CROSSCOMPILE),)
    # Not crosscompiling.
    ifeq ($(shell uname -s),Darwin)
        $(warning Elixir Circuits only works on Nerves and Linux.)
        $(warning Compiling a stub NIF for testing.)
	HAL = src/hal_stub.c
        NIF_LDFLAGS += -undefined dynamic_lookup -dynamiclib
    else
        ifeq ($(MIX_ENV),test)
            $(warning Compiling stub NIF to support 'mix test')
            HAL = src/hal_stub.c
        endif
        NIF_LDFLAGS += -fPIC -shared
    endif
else
# Crosscompiled build
NIF_LDFLAGS += -fPIC -shared
endif
HAL ?= src/hal_sysfs.c src/hal_sysfs_interrupts.c
HAL += src/nif_utils.c

# Check if we should compile in the helper
ifneq ($(TARGET_CFLAGS),)
    TARGETS += $(HELPER)
    CFLAGS += $(TARGET_CFLAGS)

    HELPER_SRC = src/helper_rpi.c
endif

CFLAGS ?= -O2 -Wall -Wextra -Wno-unused-parameter -pedantic

# Set Erlang-specific compile and linker flags
ERL_CFLAGS ?= -I$(ERL_EI_INCLUDE_DIR)
ERL_LDFLAGS ?= -L$(ERL_EI_LIBDIR)

NIF_SRC =src/gpio_nif.c $(HAL)
HEADERS =$(wildcard src/*.h)

calling_from_make:
	mix compile

all: priv $(TARGETS)

priv:
	mkdir -p priv

$(NIF): $(HEADERS)

$(NIF): $(NIF_SRC)
	$(CC) -o $@ $(NIF_SRC) $(ERL_CFLAGS) $(CFLAGS) $(ERL_LDFLAGS) $(NIF_LDFLAGS)

ifneq ($(HELPER_SRC),)
$(HELPER): $(HELPER_SRC)
	$(CC) -o $@ $(HELPER_SRC) $(CFLAGS) $(HELPER_LDFLAGS)
endif

clean:
	$(RM) $(NIF) $(HELPER)

.PHONY: all clean calling_from_make
