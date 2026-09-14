# Root Makefile for mero-monitor-#2

.PHONY: all clean probe shell

all: probe shell

probe:
	$(MAKE) -C wince/probe

shell:
	$(MAKE) -C wince/shell

clean:
	$(MAKE) -C wince/probe clean
	$(MAKE) -C wince/shell clean
