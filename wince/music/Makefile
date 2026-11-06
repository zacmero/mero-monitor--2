IMAGE = ghcr.io/enlyze/windows-ce-build-environment-arm:latest
TARGET = mero-music.exe
all: $(TARGET)
$(TARGET): mero-music.c ../mero-live.h ../mero-bus.h ../mero-shell-return.h ../mero-art.h
	docker run --rm -v "$(shell cd .. && pwd):/src" -w /src/music $(IMAGE) arm-mingw32ce-gcc -O2 -DUNICODE -D_UNICODE -D_WIN32_WCE=0x0400 mero-music.c -o $(TARGET) -lws2 -lcoredll
clean:
	rm -f $(TARGET)
test-toggle: test-toggle.c
	docker run --rm -v "$(shell cd .. && pwd):/src" -w /src/music $(IMAGE) arm-mingw32ce-gcc -Os -s -DUNICODE -D_UNICODE -D_WIN32_WCE=0x0400 test-toggle.c -o mero-music-toggle.exe -lcoredll
test-close: test-toggle.c
	docker run --rm -v "$(shell cd .. && pwd):/src" -w /src/music $(IMAGE) arm-mingw32ce-gcc -Os -s -DUNICODE -D_UNICODE -D_WIN32_WCE=0x0400 -DTEST_CLOSE_SENSOR test-toggle.c -o mero-sensor-close.exe -lcoredll
.PHONY: all clean
