# Makefile for mero-shell (Windows CE ARM)
# Target: Foston FS-460BT

TARGET      := mero-shell.exe
SRCS        := mero-shell.c
RC_SRCS     := mero-shell.rc
RES_OBJ     := mero-shell-res.o
DOCKER_IMG  := ghcr.io/enlyze/windows-ce-build-environment-arm:latest

CC_LOCAL    := $(shell which arm-mingw32ce-gcc 2>/dev/null)

CFLAGS      := -O2 -Wall -Wextra -D_WIN32_WCE=0x0400 -DUNICODE -D_UNICODE
LDFLAGS     := -Wl,--subsystem,windows -s
LIBS        := -ltoolhelp -lcoredll

.PHONY: all clean

all: $(TARGET)

ifeq ($(CC_LOCAL),)
$(TARGET): $(SRCS) $(RC_SRCS)
	@echo "Building with Docker ($(DOCKER_IMG))..."
	docker run --rm -u $(shell id -u):$(shell id -g) -v "$(CURDIR)":/work -w /work $(DOCKER_IMG) \
		sh -c "arm-mingw32ce-windres -i $(RC_SRCS) -o $(RES_OBJ) && arm-mingw32ce-gcc $(CFLAGS) -o $(TARGET) $(SRCS) $(RES_OBJ) $(LDFLAGS) $(LIBS)"
	@echo "Built $(TARGET) successfully: $$(ls -lh $(TARGET) | awk '{print $$5}')"
else
$(TARGET): $(SRCS) $(RC_SRCS)
	@echo "Building with local $(CC_LOCAL)..."
	arm-mingw32ce-windres -i $(RC_SRCS) -o $(RES_OBJ)
	arm-mingw32ce-gcc $(CFLAGS) -o $(TARGET) $(SRCS) $(RES_OBJ) $(LDFLAGS) $(LIBS)
	@echo "Built $(TARGET) successfully: $$(ls -lh $(TARGET) | awk '{print $$5}')"
endif

clean:
	rm -f $(TARGET) $(RES_OBJ)
