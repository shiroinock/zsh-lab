CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -Wno-misleading-indentation
VERSION := $(shell cat VERSION)
CPPFLAGS += -DZSHLAB_VERSION='"$(VERSION)"'
UNAME_S := $(shell uname -s)
LDLIBS = -lncurses
ifeq ($(UNAME_S),Linux)
LDLIBS += -lutil
endif
SOURCES = src/app.c src/terminal.h src/lessons.h src/shell.h VERSION

all: zsh-lab

zsh-lab: $(SOURCES)
	$(CC) $(CPPFLAGS) $(CFLAGS) src/app.c $(LDLIBS) -o $@

tests/test-native: src/tests.c $(SOURCES)
	@mkdir -p tests
	$(CC) $(CPPFLAGS) $(CFLAGS) -Wno-unused-function src/tests.c $(LDLIBS) -o $@

tests/smoke-ui: src/smoke.c $(SOURCES)
	@mkdir -p tests
	$(CC) $(CPPFLAGS) $(CFLAGS) -Wno-unused-function src/smoke.c $(LDLIBS) -o $@

test: tests/test-native
	./tests/test-native

smoke: zsh-lab tests/smoke-ui
	./tests/smoke-ui ./zsh-lab

check: all test smoke

sanitize:
	@mkdir -p tests
	$(CC) $(CPPFLAGS) -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wno-unused-function src/tests.c $(LDLIBS) -o tests/test-sanitized
	./tests/test-sanitized

release:
ifeq ($(UNAME_S),Darwin)
	$(CC) $(CPPFLAGS) $(CFLAGS) -arch arm64 -arch x86_64 -mmacosx-version-min=11.0 src/app.c $(LDLIBS) -o zsh-lab
	codesign --force --sign - zsh-lab
else
	@echo 'Universal Binary の作成は macOS 上で実行してください。'; exit 1
endif

package:
	./scripts/package-release.sh

clean:
	rm -f zsh-lab tests/test-native tests/smoke-ui tests/test-sanitized

.PHONY: all test smoke check sanitize release package clean
