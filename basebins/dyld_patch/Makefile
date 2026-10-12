CC = $(shell xcrun --sdk iphoneos --find clang)
LD = $(shell xcrun --sdk iphoneos --find ld)
OBJCOPY = $(shell which llvm-objcopy)
SDK = $(shell xcrun --sdk iphoneos --show-sdk-path)
LDID = $(shell which ldid)

CFLAGS := -I./src -target arm64-apple-ios -mios-version-min=12.0 -fvisibility=hidden -fvisibility-inlines-hidden
LDFLAGS := -nostdlib

all: $(SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) -o dyld_patch -c src/main.s
	$(OBJCOPY) --dump-section=__TEXT,__text=dyld_patch_text_12.bin dyld_patch
	@printf "12: _dyld_patch_info addr: " && nm ./dyld_patch | grep _dyld_patch_info | cut -c -16 
	@printf "12: _fcntl_hook addr: " && nm ./dyld_patch | grep _fcntl_hook | cut -c -16 
	@printf "12: _mmap_hook addr: " && nm ./dyld_patch | grep _mmap_hook | cut -c -16

	$(CC) $(CFLAGS) $(LDFLAGS) -o dyld_patch -c src/main.s -DDYLD_PATCH_IOS_13=1
	$(OBJCOPY) --dump-section=__TEXT,__text=dyld_patch_text_13.bin dyld_patch
	@printf "13: _dyld_patch_info addr: " && nm ./dyld_patch | grep _dyld_patch_info | cut -c -16 
	@printf "13: _fcntl_hook addr: " && nm ./dyld_patch | grep _fcntl_hook | cut -c -16 
	@printf "13: _mmap_hook addr: " && nm ./dyld_patch | grep _mmap_hook | cut -c -16 

clean:
	@rm -rf dyld_patch
	@rm -rf dyld_patch*.bin
	@rm -rf *.o

install: clean all
	sshpass -p alpine ssh -p 6414 root@127.0.0.1 "rm -rf /var/mobile/dyld_patch_text.bin && rm -rf /var/mobile/dyld_patch_data.bin"
	sshpass -p alpine scp -P 6414 ./dyld_patch_text.bin root@127.0.0.1:/var/mobile/dyld_patch_text.bin
	sshpass -p alpine scp -P 6414 ./dyld_patch_data.bin root@127.0.0.1:/var/mobile/dyld_patch_data.bin
