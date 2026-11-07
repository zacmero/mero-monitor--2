IMAGE = ghcr.io/enlyze/windows-ce-build-environment-arm:latest
all: mero-backlight.exe
mero-backlight.exe: mero-backlight.c ../mero-cinema-backlight.h ../mero-live.h ../mero-bus.h ../mero-shell-return.h
	docker run --rm -u $(shell id -u):$(shell id -g) -v "$(abspath ..):/src" -w /src/backlight $(IMAGE) arm-mingw32ce-gcc -Os -s -DUNICODE -D_UNICODE -D_WIN32_WCE=0x0400 mero-backlight.c -o $@ -lws2 -lcoredll
clean:
	rm -f mero-backlight.exe
.PHONY: all clean
