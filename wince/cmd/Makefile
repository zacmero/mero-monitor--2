# Makefile for mero-cmd (Windows CE ARM)
# Target: Foston FS-460BT

TARGET      := mero-cmd.exe
SRCS        := mero-cmd.c
DOCKER_IMG  := ghcr.io/enlyze/windows-ce-build-environment-arm:latest

CC_LOCAL    := $(shell which arm-mingw32ce-gcc 2>/dev/null)

CFLAGS      := -O2 -Wall -Wextra -D_WIN32_WCE=0x0400 -DUNICODE -D_UNICODE
LDFLAGS     := -Wl,--subsystem,windows -s
LIBS        := -ltoolhelp -lcoredll

.PHONY: all clean

all: $(TARGET)

ifeq ($(CC_LOCAL),)
$(TARGET): $(SRCS)
	@echo "Local arm-mingw32ce-gcc not found. Building with Docker ($(DOCKER_IMG))..."
	docker run --rm -u $(shell id -u):$(shell id -g) -v "$(CURDIR)":/work -w /work $(DOCKER_IMG) \
		arm-mingw32ce-gcc $(CFLAGS) -o $(TARGET) $(SRCS) $(LDFLAGS) $(LIBS)
	@echo "Built $(TARGET) successfully: $$(ls -lh $(TARGET) | awk '{print $$5}')"
else
$(TARGET): $(SRCS)
	@echo "Building with local $(CC_LOCAL)..."
	arm-mingw32ce-gcc $(CFLAGS) -o $(TARGET) $(SRCS) $(LDFLAGS) $(LIBS)
	@echo "Built $(TARGET) successfully: $$(ls -lh $(TARGET) | awk '{print $$5}')"
endif

clean:
	rm -f $(TARGET)
