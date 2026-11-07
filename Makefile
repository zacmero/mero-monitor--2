# Root Makefile for mero-monitor-#2

.PHONY: all clean probe shell cmd flash gallery tuner media cam bluetooth music ambient backlight check

all: probe shell cmd flash gallery tuner media cam bluetooth music ambient backlight

backlight:
	$(MAKE) -C wince/backlight

ambient:
	$(MAKE) -C wince/ambient

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
music:
	$(MAKE) -C wince/music

check:
	python3 scripts/check_firmware_probe.py
	python3 scripts/check_backlight_app.py
	python3 scripts/check_backlight_script.py
	cc scripts/check_backlight.c -o /tmp/mero-check-backlight
	/tmp/mero-check-backlight
	/tmp/mero-check-backlight recover
	python3 scripts/check_ambient.py
	python3 scripts/check_agent_manual.py
	python3 scripts/check_startup.py
	cc -std=c99 -fshort-wchar scripts/check_usb.c -o /tmp/mero-check-usb
	/tmp/mero-check-usb
	python3 scripts/check_art.py
	python3 scripts/check_music.py
	python3 scripts/check_launcher.py
	python3 scripts/check_live_feed.py
	python3 scripts/check_companion.py
	python3 scripts/check_agent.py
	cc -Wall -Wextra -Werror scripts/check_obex.c -o /tmp/mero-check-obex
	/tmp/mero-check-obex

clean:
	$(MAKE) -C wince/backlight clean
	$(MAKE) -C wince/probe clean
	$(MAKE) -C wince/shell clean
	$(MAKE) -C wince/cmd clean
	$(MAKE) -C wince/flash clean
	$(MAKE) -C wince/gallery clean
	$(MAKE) -C wince/tuner clean
	$(MAKE) -C wince/media clean
	$(MAKE) -C wince/cam clean
	$(MAKE) -C wince/bluetooth clean
	$(MAKE) -C wince/music clean
	$(MAKE) -C wince/ambient clean
