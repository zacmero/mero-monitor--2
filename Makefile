# Root Makefile for mero-monitor-#2

.PHONY: all clean probe

all: probe

probe:
	$(MAKE) -C wince/probe

clean:
	$(MAKE) -C wince/probe clean
