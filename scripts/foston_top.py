#!/usr/bin/env python3
"""Compact process monitor for the Foston terminal only (q to exit)."""
import curses
import subprocess

def processes():
    text=subprocess.check_output(['ps','-eo','pid=,pcpu=,rss=,comm=','--sort=-pcpu'],text=True,timeout=2)
    return [(int(pid),float(cpu),int(rss)/1024,name) for line in text.splitlines() if len(parts:=line.split(None,3))==4 for pid,cpu,rss,name in [parts]]

def monitor(screen):
    try:curses.curs_set(0)
    except curses.error:pass
    screen.timeout(1500)
    while True:
        height,width=screen.getmaxyx();screen.erase()
        def line(y,text):
            if y<height:
                try:screen.addnstr(y,0,text,max(0,width-1))
                except curses.error:pass
        line(0,'PID    CPU%   MiB NAME')
        try:
            for y,(pid,cpu,memory,name) in enumerate(processes()[:max(0,height-2)],1):
                line(y,f'{pid:<6} {cpu:4.1f} {memory:5.0f} {name}')
        except (OSError,ValueError,subprocess.SubprocessError):line(1,'Process list unavailable')
        line(height-1,'q: exit | CPU = lifetime avg')
        screen.refresh()
        if screen.getch() in (ord('q'),ord('Q'),27):break

if __name__=='__main__':
    try:curses.wrapper(monitor)
    except KeyboardInterrupt:pass
