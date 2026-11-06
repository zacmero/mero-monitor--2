IMAGE = ghcr.io/enlyze/windows-ce-build-environment-arm:latest
TARGET = mero-bt.exe
all: $(TARGET)
$(TARGET): mero-bt.c ../mero-obex.h ../mero-live.h
	docker run --rm -v "$(abspath ..):/src" -w /src/bluetooth $(IMAGE) arm-mingw32ce-gcc -O2 -Wall -Wextra -DUNICODE -D_UNICODE -D_WIN32_WCE=0x0400 mero-bt.c -o $(TARGET) -lws2 -lcoredll
clean:
	rm -f $(TARGET)
.PHONY: all clean
