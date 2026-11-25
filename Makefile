# Needs mingw-w64 for 32-bit Windows (Debian/Ubuntu: gcc-mingw-w64-i686) and, for `release`, python3.
CC = i686-w64-mingw32-gcc
DELAY_MS ?= 250
CFLAGS = -Os -std=gnu99 -Wall -Wextra -Wno-cast-function-type -fno-asynchronous-unwind-tables -DDELAY_MS=$(DELAY_MS)
LDFLAGS = -shared -s -nostdlib -Wl,-e,_DllMain@12 -Wl,--enable-stdcall-fixup -Wl,--kill-at \
  -Wl,--no-insert-timestamp -Wl,--dynamicbase -Wl,--nxcompat

all: dist/dsound.dll

dist/dsound.dll: src/dsound.c src/dsound.def Makefile
	mkdir -p dist
	$(CC) $(CFLAGS) -o $@ src/dsound.c src/dsound.def $(LDFLAGS) -lkernel32

# The test program is named like the game, because the fix only works in CoDWaW.exe.
# Run dist/test/CoDWaW.exe on Windows with the DirectX runtime's XAudio2 2.0 installed; it is silent.
test: dist/dsound.dll test/test.c
	mkdir -p dist/test
	$(CC) -O1 -Wall -static-libgcc -o dist/test/CoDWaW.exe test/test.c -ldsound -lole32
	cp dist/test/CoDWaW.exe dist/test/CoDWaWmp.exe
	cp dist/dsound.dll dist/test/

release: dist/dsound.dll
	rm -rf dist/waw-dialog-fix dist/waw-dialog-fix.zip
	mkdir dist/waw-dialog-fix
	cp dist/dsound.dll README.md LICENSE dist/waw-dialog-fix/
	cd dist && python3 -m zipfile -c waw-dialog-fix.zip waw-dialog-fix && sha256sum dsound.dll waw-dialog-fix.zip > SHA256SUMS

clean:
	rm -rf dist

.PHONY: all test release clean
