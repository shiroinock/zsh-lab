CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -Wno-misleading-indentation -Wno-unused-function
VERSION := $(shell cat VERSION)
CPPFLAGS += -DZSHLAB_VERSION='"$(VERSION)"'
UNAME_S := $(shell uname -s)
LDLIBS = -lncurses
ifeq ($(UNAME_S),Linux)
LDLIBS += -lutil
endif
SOURCES = src/app.c src/terminal.h src/lessons.h src/shell.h src/isolation.h VERSION

all: zsh-lab

src/image_context.h: $(SOURCES) container/Dockerfile scripts/embed-image.py
	python3 scripts/embed-image.py

zsh-lab: $(SOURCES) src/launcher.h src/image_context.h
	$(CC) $(CPPFLAGS) $(CFLAGS) src/app.c $(LDLIBS) -o $@

tests/test-native: src/tests.c $(SOURCES)
	@mkdir -p tests
	$(CC) $(CPPFLAGS) $(CFLAGS) src/tests.c $(LDLIBS) -o $@

tests/smoke-ui: src/smoke.c $(SOURCES)
	@mkdir -p tests
	$(CC) $(CPPFLAGS) $(CFLAGS) src/smoke.c $(LDLIBS) -o $@

tests/test-launcher: src/launcher_tests.c $(SOURCES) src/launcher.h src/image_context.h
	@mkdir -p tests
	$(CC) $(CPPFLAGS) $(CFLAGS) src/launcher_tests.c $(LDLIBS) -o $@

host-check: all tests/test-launcher
	./tests/test-launcher
	./zsh-lab --version
	./zsh-lab --list >/dev/null

test: zsh-lab
	./scripts/check-docker.sh

smoke: zsh-lab tests/smoke-ui
	./tests/smoke-ui ./zsh-lab

check: host-check test smoke

sanitize: src/image_context.h
	@mkdir -p tests
	$(CC) $(CPPFLAGS) -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wno-unused-function src/launcher_tests.c $(LDLIBS) -o tests/test-sanitized
	./tests/test-sanitized

release: src/image_context.h
ifeq ($(UNAME_S),Darwin)
	$(CC) $(CPPFLAGS) $(CFLAGS) -arch arm64 -mmacosx-version-min=11.0 src/app.c $(LDLIBS) -o zsh-lab
	codesign --force --sign - zsh-lab
else
	@echo 'macOS arm64 バイナリの作成は macOS 上で実行してください。'; exit 1
endif

package:
	./scripts/package-release.sh

clean:
	rm -f zsh-lab tests/test-native tests/smoke-ui tests/test-sanitized tests/test-launcher src/image_context.h

.PHONY: all test smoke check host-check sanitize release package clean
