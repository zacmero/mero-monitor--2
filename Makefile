# Root Makefile for mero-monitor-#2

.PHONY: all clean probe shell cmd flash gallery tuner media

all: probe shell cmd flash gallery tuner media

probe:
	$(MAKE) -C wince/probe

shell:
	$(MAKE) -C wince/shell

cmd:
	$(MAKE) -C wince/cmd

flash:
	$(MAKE) -C wince/flash

gallery:
	$(MAKE) -C wince/gallery

tuner:
	$(MAKE) -C wince/tuner

media:
	$(MAKE) -C wince/media

clean:
	$(MAKE) -C wince/probe clean
	$(MAKE) -C wince/shell clean
	$(MAKE) -C wince/cmd clean
	$(MAKE) -C wince/flash clean
	$(MAKE) -C wince/gallery clean
	$(MAKE) -C wince/tuner clean
	$(MAKE) -C wince/media clean
