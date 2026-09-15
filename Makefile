# Root Makefile for mero-monitor-#2

.PHONY: all clean probe shell cmd flash

all: probe shell cmd flash

probe:
	$(MAKE) -C wince/probe

shell:
	$(MAKE) -C wince/shell

cmd:
	$(MAKE) -C wince/cmd

flash:
	$(MAKE) -C wince/flash

clean:
	$(MAKE) -C wince/probe clean
	$(MAKE) -C wince/shell clean
	$(MAKE) -C wince/cmd clean
	$(MAKE) -C wince/flash clean
