#!/usr/bin/env python3
"""Exercise real tmux input, signed dispatch/dedup and keyboard acknowledgements."""
import base64
import json
from pathlib import Path
import secrets
import socket
import subprocess
import tempfile
import time
from types import SimpleNamespace
from unittest.mock import patch
import companion_live as live
from companion_terminal import Terminal, TARGETS
from companion_keyboard import Layout,Keyboard,EVENT


def check():
    layout=Layout('us')
    try:
        assert layout.event(30,1)==b'a';layout.event(30,0)
        assert layout.event(30,1)==b'a';assert layout.event(30,1)==b'';assert layout.event(30,2)==b'a';layout.event(30,0)
        layout.event(42,1);assert layout.event(30,1)==b'A';layout.event(30,0);layout.event(42,0)
        layout.event(29,1);assert layout.event(46,1)==b'\x03';layout.event(46,0);layout.event(29,0)
        assert layout.event(103,1)==b'\x1b[A'
    finally:layout.close()
    mirrored=Keyboard();mirrored.fds=[100,101];mirrored.session='test';mirrored.layout=Layout('us')
    events=EVENT.pack(0,0,1,30,1)+EVENT.pack(0,1,1,30,1)+EVENT.pack(0,2,1,30,0)
    remaining={100:[events],101:[events]}
    def read(fd,cap):
        if remaining[fd]:return remaining[fd].pop()
        raise BlockingIOError()
    try:
        with patch('companion_keyboard.os.read',side_effect=read),patch('companion_keyboard.subprocess.run',return_value=SimpleNamespace(stdout='Active=yes\nLockedHint=no\n')):
            mirrored.poll()
        assert mirrored.queue==b'a' and mirrored.typing_fd==100
    finally:mirrored.fds=[];mirrored.layout.close()
    keyboard=Keyboard();keyboard.queue=b'x'*31+b'\xc3\xa1';assert keyboard.packet()==b'x'*31 and keyboard.packet()==b'\xc3\xa1'
    key=bytes(range(32))
    with tempfile.TemporaryDirectory() as directory:
        root=Path(directory);share=root/'share';share.mkdir();private=root/'private';private.mkdir()
        (private/'key').write_text(key.hex())
        t=Terminal(share,lambda:key,state=root/'terminal',endpoint=root/'socket',server='mero-check-'+secrets.token_hex(4))
        try:
            assert t.action(dict(action='terminal-open',track='wrong',value=0))=='session_changed_press_again'
            assert not t.active
            assert t.action(dict(action='terminal-view',track=t.token,value=1))=='ok'
            until=time.monotonic()+8
            while time.monotonic()<until:
                screen=t.tmux('capture-pane','-p','-t','foston:0.0').decode()
                if 'mero:~$' in screen:break
                time.sleep(.05)
            assert 'ArchMerOS' in screen and 'RAM:' in screen and 'mero:~$' in screen,screen
            feed=live.BluetoothLive(share,private);feed.terminal=t
            event=dict(v=1,device=live.DEVICE,id='100-1-1',created=int(time.time()),action='terminal-input',
                       track=t.token+'.'+b"printf 'ROUNDTRIP_OK\\n'\r".hex(),value=0)
            (share/'live-event.txt').write_bytes(live.signed(event,key))
            def after_input():
                assert feed.result=='ok'
                return t.update()
            feed.publish([after_input]);assert feed.result=='ok'
            feed.publish([])
            # Restarting the host dispatcher must not type the command again.
            fresh=live.BluetoothLive(share,private);fresh.terminal=t;fresh.publish([]);assert fresh.result=='ok'
            until=time.monotonic()+3
            while time.monotonic()<until:
                screen=t.tmux('capture-pane','-p','-t','foston:0.0').decode()
                if '\nROUNDTRIP_OK\n' in screen:break
                time.sleep(.05)
            assert screen.count('\nROUNDTRIP_OK\n')==1,screen
            line=t.update();fields=dict(item.split('=',1) for item in line[10:].split('|'))
            assert 'ROUNDTRIP_OK' in base64.b64decode(fields['text']).decode()
            for view in range(4):
                assert t.action(dict(action='terminal-view',track=t.token,value=view))=='ok'
                assert len(t.update())<1700
            assert t.action(dict(action='terminal-view',track=t.token,value=9))=='invalid_terminal_view'
        finally:
            t.close()
            try:t.tmux('kill-server')
            except subprocess.SubprocessError:pass

    with tempfile.TemporaryDirectory() as directory:
        root=Path(directory);t=Terminal(root,lambda:key,state=root/'state',endpoint=root/'sock')
        def mode(action,owner='0123456789abcdef'):
            with socket.socket(socket.AF_UNIX,socket.SOCK_STREAM) as connection:
                connection.connect(str(root/'sock'))
                connection.sendall(json.dumps(dict(mode=action,owner=owner)).encode()+b'\n')
                t.poll_keyboard()
                return connection.recv(128).decode().strip()
        assert mode('engage')=='physical'
        assert mode('release','1123456789abcdef')=='touch' and t.physical
        client=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM)
        try:
            client.connect(str(root/'sock'));client.sendall(json.dumps(dict(target=TARGETS['terminal'],hex='61620d')).encode()+b'\n')
            field=t.keyboard_field();assert ':MERO_TERMINAL:61620d' in field
            identifier=field.split(':')[0]
            (root/'keyboard-result.txt').write_bytes(live.signed(dict(v=1,device=live.DEVICE,id=identifier,result='queued'),key))
            assert t.keyboard_field()==''
            assert client.recv(128)==b'queued\n'
            assert mode('status')=='physical'
            assert t.action(dict(action='terminal-touch',track=t.token,value=0))=='ok'
            assert mode('status')=='touch' and not t.physical
        finally:client.close();t.close()
    subprocess.run(['cc','scripts/check_keyboard.c','-o','/tmp/mero-check-keyboard'],check=True)
    subprocess.run(['/tmp/mero-check-keyboard'],check=True)
    source=Path('wince/terminal/mero-terminal.c').read_text()
    selected=source[source.index('static const char *letters'):source.index('static void HostLine(')]
    selected+=source[source.index('static void Flush('):source.index('static HFONT Font(')]
    selected+=source[source.index('static int KeyHeight('):source.index('static void Paint(')]
    with tempfile.TemporaryDirectory() as directory:
        root=Path(directory);(root/'terminal_native.inc').write_text(selected)
        subprocess.run(['cc','-I',str(root),'scripts/check_terminal_native.c','-o',str(root/'check')],check=True)
        subprocess.run([str(root/'check')],check=True)
    print('PASS: real tmux greeting/input, signed terminal routing/dedup, bounded views and PC keyboard acknowledgement')


if __name__=='__main__':check()
