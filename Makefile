CC ?= gcc
STATIC ?= 0

UNAME_S := $(shell uname -s 2>/dev/null || echo Unknown)
IS_WINDOWS := $(shell echo "$(UNAME_S)" | grep -qiE 'mingw|msys|cygwin' && echo 1 || echo 0)

PKG_CONFIG := $(shell command -v pkg-config 2>/dev/null)

PKG_CFLAGS := $(shell test -n "$(PKG_CONFIG)" && $(PKG_CONFIG) --cflags gtk+-2.0 libcurl 2>/dev/null)

ifeq ($(STATIC),1)
  ifeq ($(IS_WINDOWS),1)
    PKG_LIBS := $(shell test -n "$(PKG_CONFIG)" && $(PKG_CONFIG) --libs gtk+-2.0 2>/dev/null) \
	$(shell test -n "$(PKG_CONFIG)" && $(PKG_CONFIG) --static --libs libcurl 2>/dev/null)
  else
    PKG_LIBS := $(shell test -n "$(PKG_CONFIG)" && $(PKG_CONFIG) --libs gtk+-2.0 libcurl 2>/dev/null)
  endif
else
  PKG_LIBS := $(shell test -n "$(PKG_CONFIG)" && $(PKG_CONFIG) --libs gtk+-2.0 libcurl 2>/dev/null)
endif

BASE_CFLAGS := -Wall -Wextra -std=c99 -Isrc -Ithird_party -Wno-deprecated-declarations $(PKG_CFLAGS)
CFLAGS ?= -O2
ALL_CFLAGS = $(CFLAGS) $(BASE_CFLAGS)
LDFLAGS ?=
LIBS := $(PKG_LIBS) -lm

ifeq ($(STATIC),1)
  ifneq ($(UNAME_S),Darwin)
    LDFLAGS += -static-libgcc
  endif
  ifeq ($(IS_WINDOWS),1)
    LDFLAGS += -static-libstdc++
    LIBS += -Wl,-Bstatic -lwinpthread -Wl,-Bdynamic
  endif
endif

SRCS := \
	src/main.c \
	src/http.c \
	src/log.c \
	src/history.c \
	src/settings.c \
	src/weather.c \
	src/icon.c \
	src/ui.c \
	third_party/cJSON.c

OBJS := $(SRCS:.c=.o)

TEST_LIB_SRCS := \
	src/http.c \
	src/log.c \
	src/history.c \
	src/settings.c \
	src/weather.c \
	src/icon.c \
	third_party/cJSON.c

TEST_LIB_OBJS := $(TEST_LIB_SRCS:.c=.o)

TEST_SRCS := \
	tests/run_tests.c \
	tests/test_weather.c \
	tests/test_settings.c \
	tests/test_history.c \
	tests/test_http.c \
	tests/test_icon.c

TEST_OBJS := $(TEST_SRCS:.c=.o)

# Drop leftover MinGW/COFF objects after a win32 cross-build so native link works.
ifeq ($(IS_WINDOWS),0)
_ := $(shell for o in $(OBJS) $(TEST_LIB_OBJS) $(TEST_OBJS); do \
	[ -f "$$o" ] || continue; \
	file -b "$$o" 2>/dev/null | grep -qE 'ELF|Mach-O' || rm -f "$$o"; \
done)
endif

.PHONY: all clean run lint test

all: c-weather-gtk2

c-weather-gtk2: $(OBJS)
ifeq ($(IS_WINDOWS),1)
	$(CC) $(LDFLAGS) -mwindows -o $@ $(OBJS) $(LIBS)
else
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)
endif

%.o: %.c
	$(CC) $(ALL_CFLAGS) -c -o $@ $<

tests/%.o: tests/%.c
	$(CC) $(ALL_CFLAGS) -Itests -c -o $@ $<

run: c-weather-gtk2
	./c-weather-gtk2

test: c-weather-gtk2-tests
	./c-weather-gtk2-tests

c-weather-gtk2-tests: $(TEST_OBJS) $(TEST_LIB_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(TEST_OBJS) $(TEST_LIB_OBJS) $(LIBS)

lint:
	cppcheck --error-exitcode=1 --enable=warning,style,performance,portability \
		--suppressions-list=cppcheck-suppressions.txt \
		-I src -I third_party src/

clean:
	rm -f $(OBJS) $(TEST_OBJS) \
		c-weather-gtk2 c-weather-gtk2.exe c-weather-gtk2-tests c-weather-gtk2-tests.exe
