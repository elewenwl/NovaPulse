CC = $(shell xcrun --sdk iphoneos --find clang)
STRIP = $(shell xcrun --sdk iphoneos --find strip)
SDK = $(shell xcrun --sdk iphoneos --show-sdk-path)
OBJCOPY = $(shell which llvm-objcopy)
LDID = $(shell which ldid)

CFLAGS := -I./include -isysroot $(SDK) -mios-version-min=12.0 -arch arm64e -arch arm64 \
	-Werror -Wall -Wno-unused-variable -Wno-unused-function -fvisibility=hidden -fvisibility-inlines-hidden
LDFLAGS := -L$(SDK)/usr/lib -lc -lSystem -lmis -lcompression -framework CoreFoundation

SRC = $(wildcard src/*.c) $(wildcard src/*.s)

all: $(SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) -o jbutil $^
	$(STRIP) -x jbutil
	$(LDID) -Sents.xml jbutil

clean:
	@rm -rf jbutil
	@rm -rf jbutil.dSYM
	@rm -rf *.o
