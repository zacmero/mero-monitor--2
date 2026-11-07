IMAGE = ghcr.io/enlyze/windows-ce-build-environment-arm:latest
TARGET = mero-ambient.exe
all: $(TARGET)
$(TARGET): mero-ambient.c ../mero-live.h ../mero-bus.h ../mero-ambient.h ../mero-shell-return.h
	docker run --rm -v "$(shell cd .. && pwd):/src" -w /src/ambient $(IMAGE) arm-mingw32ce-gcc -Os -s -DUNICODE -D_UNICODE -D_WIN32_WCE=0x0400 mero-ambient.c -o $(TARGET) -lws2 -lcoredll
clean:
	rm -f $(TARGET)
.PHONY: all clean
