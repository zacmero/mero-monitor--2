# Root Makefile for mero-monitor-#2

.PHONY: all clean probe shell cmd flash gallery tuner media cam bluetooth check

all: probe shell cmd flash gallery tuner media cam bluetooth

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

cam:
	$(MAKE) -C wince/cam

bluetooth:
	$(MAKE) -C wince/bluetooth

check:
	python3 scripts/check_companion.py
	cc -Wall -Wextra -Werror scripts/check_obex.c -o /tmp/mero-check-obex
	/tmp/mero-check-obex

clean:
	$(MAKE) -C wince/probe clean
	$(MAKE) -C wince/shell clean
	$(MAKE) -C wince/cmd clean
	$(MAKE) -C wince/flash clean
	$(MAKE) -C wince/gallery clean
	$(MAKE) -C wince/tuner clean
	$(MAKE) -C wince/media clean
	$(MAKE) -C wince/cam clean
	$(MAKE) -C wince/bluetooth clean
