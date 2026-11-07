IMAGE = ghcr.io/enlyze/windows-ce-build-environment-arm:latest
TARGET = mero-bt.exe
all: $(TARGET)
$(TARGET): mero-bt.c ../mero-obex.h ../mero-live.h ../mero-bus.h ../mero-shell-return.h ../mero-agent-protocol.h ../mero-auth.h ../mero-live-codec.h
	docker run --rm -v "$(abspath ..):/src" -w /src/bluetooth $(IMAGE) arm-mingw32ce-gcc -O2 -Wall -Wextra -DUNICODE -D_UNICODE -D_WIN32_WCE=0x0400 mero-bt.c -o $(TARGET) -lws2 -lcoredll
clean:
	rm -f $(TARGET) mero-agent.exe

agent: mero-bt.c mero-agent-runtime.h mero-keyboard.h mero-agent-key.h ../mero-agent-protocol.h ../mero-auth.h ../mero-obex.h ../mero-live.h ../mero-bus.h ../mero-shell-return.h ../mero-live-codec.h ../mero-art.h ../mero-ambient.h ../mero-photo-codec.h ../mero-live-zlib.h ../gallery/stb_image.h ../mero-cinema-backlight.h
	docker run --rm -v "$(abspath ..):/src" -w /src/bluetooth $(IMAGE) arm-mingw32ce-gcc -O2 -Wall -Wextra -DUNICODE -D_UNICODE -D_WIN32_WCE=0x0400 -DMERO_AGENT_BUILD mero-bt.c -o mero-agent.exe -lws2 -lcoredll -ltoolhelp

agent-upgrade: mero-agent-upgrade.c mero-upgrade-plan.h ../mero-agent-protocol.h ../mero-auth.h
	docker run --rm -v "$(abspath ..):/src" -w /src/bluetooth $(IMAGE) arm-mingw32ce-gcc -O2 -Wall -Wextra -DUNICODE -D_UNICODE -D_WIN32_WCE=0x0400 mero-agent-upgrade.c -o mero-agent-upgrade.exe -lcoredll -ltoolhelp
.PHONY: all clean agent agent-upgrade
