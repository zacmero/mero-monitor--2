IMAGE = ghcr.io/enlyze/windows-ce-build-environment-arm:latest
TARGET = mero-pc-terminal.exe
all: $(TARGET)
$(TARGET): mero-terminal.c mero-terminal-font.h ../mero-live.h ../mero-bus.h ../mero-shell-return.h
	docker run --rm -u $(shell id -u):$(shell id -g) -v "$(shell cd .. && pwd):/src" -w /src/terminal $(IMAGE) arm-mingw32ce-gcc -Os -s -DUNICODE -D_UNICODE -D_WIN32_WCE=0x0400 mero-terminal.c -o $(TARGET) -lws2 -lcoredll
clean:
	rm -f $(TARGET)
.PHONY: all clean
