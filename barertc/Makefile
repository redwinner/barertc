#
# Makefile
#
#
#

PROJECT	  := barertc
VERSION   := 1.0.0

LIBRE_MK  := $(shell [ -f ../re/mk/re.mk ] && \
	echo "../re/mk/re.mk")
ifeq ($(LIBRE_MK),)
LIBRE_MK  := $(shell [ -f /usr/share/re/re.mk ] && \
	echo "/usr/share/re/re.mk")
endif
ifeq ($(LIBRE_MK),)
LIBRE_MK  := $(shell [ -f /usr/local/share/re/re.mk ] && \
	echo "/usr/local/share/re/re.mk")
endif

include $(LIBRE_MK)

LIBREM_PATH	:= $(shell [ -d ../rem ] && echo "../rem")

INSTALL := install
ifeq ($(DESTDIR),)
PREFIX  := /usr/local
else
PREFIX  := /usr
endif
BINDIR	:= $(PREFIX)/bin
CFLAGS	+= -I$(LIBRE_INC) -Iinclude
CFLAGS  += -I$(LIBREM_PATH)/include -I$(SYSROOT)/local/include/rem
BIN	:= $(PROJECT)$(BIN_SUFFIX)
APP_MK	:= src/srcs.mk

BRTC_MK	:= src/brtc/brtc.mk

ifneq ($(LIBREM_PATH),)
LIBS    += -L$(LIBREM_PATH)
endif

LIBS    += -lbaresip -lrem -lm


include $(APP_MK)
include $(BRTC_MK)

OBJS	?= $(patsubst %.c,$(BUILD)/src/%.o,$(SRCS))

all: $(BIN)

-include $(OBJS:.o=.d)

$(BIN): $(OBJS)
	@echo "  LD      $@"
	@$(LD) $(LFLAGS) $^ -L$(LIBRE_SO) -lre $(LIBS) -o $@

$(BUILD)/%.o: %.c $(BUILD) Makefile $(APP_MK) $(BRTC_MK)
	@echo "  CC      $@"
	@$(CC) $(CFLAGS) -o $@ -c $< $(DFLAGS)

$(BUILD): Makefile
	@mkdir -p $(BUILD)/src/brtc
	@touch $@

clean:
	@rm -rf $(BIN) $(BUILD)

install: $(BIN)
	@mkdir -p $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 0755 $(BIN) $(DESTDIR)$(BINDIR)
