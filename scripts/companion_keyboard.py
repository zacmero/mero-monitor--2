"""Background keyboard forwarding inside the existing bridge, with evdev grabs.

Uses the installed libxkbcommon and current Hyprland layout. Devices are opened
only while explicitly engaged from the Foston or its shortcut; no key log is retained.
"""
import ctypes as C
import ctypes.util
import fcntl
import glob
import json
import os
from pathlib import Path
import struct
import subprocess
import time

EVENT=struct.Struct('@llHHi')
# Linux input.h: _IOW('E',0x90,int), _IOC(_IOC_READ,'E',0x18,96).
GRAB=(1<<30)|(C.sizeof(C.c_int)<<16)|(ord('E')<<8)|0x90
KEYS=(2<<30)|(96<<16)|(ord('E')<<8)|0x18
SPECIAL={1:b'\x1b',14:b'\x7f',15:b'\t',28:b'\r',96:b'\r',
         102:b'\x1b[H',103:b'\x1b[A',104:b'\x1b[5~',105:b'\x1b[D',
         106:b'\x1b[C',107:b'\x1b[F',108:b'\x1b[B',109:b'\x1b[6~',111:b'\x1b[3~'}


class KeysHeld(RuntimeError):
    """Arming waits for shortcut keys to be released before grabbing."""


class Layout:
    def __init__(self, layout='us', variant='', options='', active=''):
        self.lib=C.CDLL(ctypes.util.find_library('xkbcommon'))
        def fn(name,result,args):
            f=getattr(self.lib,name);f.restype=result;f.argtypes=args;return f
        ptr=C.c_void_p;u=C.c_uint32
        self.context_new=fn('xkb_context_new',ptr,[C.c_int])
        self.map_new=fn('xkb_keymap_new_from_names',ptr,[ptr,ptr,C.c_int])
        self.state_new=fn('xkb_state_new',ptr,[ptr])
        self.key=fn('xkb_state_update_key',C.c_int,[ptr,u,C.c_int])
        self.utf8=fn('xkb_state_key_get_utf8',C.c_int,[ptr,u,ptr,C.c_size_t])
        self.mod=fn('xkb_state_mod_name_is_active',C.c_int,[ptr,C.c_char_p,C.c_int])
        self.context_free=fn('xkb_context_unref',None,[ptr])
        self.map_free=fn('xkb_keymap_unref',None,[ptr])
        self.state_free=fn('xkb_state_unref',None,[ptr])
        self.layout_name=fn('xkb_keymap_layout_get_name',C.c_char_p,[ptr,u])
        self.mask=fn('xkb_state_update_mask',C.c_int,[ptr,u,u,u,u,u,u])
        class Names(C.Structure):
            _fields_=[(n,C.c_char_p) for n in ['rules','model','layout','variant','options']]
        names=Names(None,None,layout.encode(),variant.encode(),options.encode())
        self.context=self.context_new(0);self.keymap=self.map_new(self.context,C.byref(names),0)
        if not self.keymap:raise RuntimeError('Cannot load keyboard layout')
        self.state=self.state_new(self.keymap)
        for index in range(len(layout.split(','))):
            name=self.layout_name(self.keymap,index)
            if name and name.decode()==active:self.mask(self.state,0,0,0,0,0,index)
        self.held=set()

    def event(self,code,value):
        if value==0:self.held.discard(code);self.key(self.state,code+8,0);return b''
        if value==1 and code in self.held:return b''
        if value==2 and code not in self.held:return b''
        if value==1:self.held.add(code);self.key(self.state,code+8,1)
        if code==110 and self.held.intersection({125,126}) and self.held.intersection({56,100}):
            raise RuntimeError('PC keyboard released')
        if code==1 and self.held.intersection({29,97}) and self.held.intersection({56,100}):
            raise RuntimeError('PC keyboard released')
        if code in SPECIAL:return SPECIAL[code]
        buf=C.create_string_buffer(16);n=self.utf8(self.state,code+8,buf,len(buf))
        data=buf.raw[:n] if 0<n<len(buf) else b''
        if data and self.mod(self.state,b'Mod1',8):data=b'\x1b'+data
        return data

    def close(self):
        self.state_free(self.state);self.map_free(self.keymap);self.context_free(self.context)


def hyprland_command(session,proc_root=Path("/proc")):
    """Resolve the compositor for the current local session, even after boot.

    User services can predate Hyprland and lack its instance environment. Never
    pick another user's compositor or a stale/different login session.
    """
    result=subprocess.run(['hyprctl','instances','-j'],capture_output=True,text=True,timeout=.5,check=True)
    candidates=[]
    for instance in json.loads(result.stdout):
        try:
            pid=int(instance['pid']);name=instance['instance']
            if pid<=0 or not isinstance(name,str) or not name or len(name)>200 or any(c not in 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.-' for c in name) or name.startswith('-'):continue
            process=proc_root/str(pid)
            if process.stat().st_uid!=os.getuid():continue
            environment=dict(item.split(b'=',1) for item in (process/'environ').read_bytes().split(b'\0') if b'=' in item)
            if environment.get(b'XDG_SESSION_ID')==session.encode():candidates.append(name)
        except (OSError,ValueError,KeyError,TypeError):continue
    if len(candidates)!=1:raise RuntimeError('Cannot identify active local Hyprland session')
    return ['hyprctl','-i',candidates[0]]


class Keyboard:
    def __init__(self):
        self.fds=[];self.layout=None;self.queue=b'';self.session='';self.typing_fd=None;self.next_seat_check=0

    def start(self):
        if self.fds:return
        config=Path.home()/'.config/mero/foston-keyboard.json'
        if config.exists():
            paths=json.loads(config.read_text())['devices']
            if not isinstance(paths,list) or not paths or any(not isinstance(p,str) or not p.startswith('/dev/input/by-id/') for p in paths):
                raise ValueError('Invalid keyboard device configuration')
        else:
            paths=glob.glob('/dev/input/by-id/usb-2a7a_CASUE_USB_KB*event-kbd')
        if not paths:raise RuntimeError('PC keyboard not found')
        self.session=subprocess.run(['loginctl','show-user',str(os.getuid()),'-p','Display','--value'],
                                    capture_output=True,text=True,timeout=.5,check=True).stdout.strip()
        if not self.session:raise RuntimeError('No local desktop session')
        result=subprocess.run(['loginctl','show-session',self.session,'-p','Active','-p','LockedHint'],capture_output=True,text=True,timeout=.5,check=True)
        state=dict(line.split('=',1) for line in result.stdout.splitlines())
        if state.get('Active')!='yes' or state.get('LockedHint')!='no':raise RuntimeError('Desktop locked or inactive - PC keyboard released')
        hyprctl=hyprland_command(self.session)
        options={}
        for key in ['kb_layout','kb_variant','kb_options']:
            r=subprocess.run(hyprctl+['getoption','input:'+key,'-j'],capture_output=True,text=True,timeout=.5,check=True)
            options[key]=json.loads(r.stdout).get('str','')
        active=''
        r=subprocess.run(hyprctl+['devices','-j'],capture_output=True,text=True,timeout=.5,check=True)
        for keyboard in json.loads(r.stdout).get('keyboards',[]):
            if keyboard.get('main'):active=keyboard.get('active_keymap','')
        self.layout=Layout(options['kb_layout'] or 'us',options['kb_variant'],options['kb_options'],active)
        try:
            for path in paths:
                fd=os.open(path,os.O_RDONLY|os.O_NONBLOCK)
                self.fds.append(fd)
                held=bytearray(96);fcntl.ioctl(fd,KEYS,held,True)
                if any(held):raise KeysHeld('Release shortcut keys to enable PC keyboard')
                fcntl.ioctl(fd,GRAB,1)
        except Exception:
            self.close();raise

    def poll(self):
        if self.fds and time.monotonic()>=self.next_seat_check:
            result=subprocess.run(['loginctl','show-session',self.session,'-p','Active','-p','LockedHint'],
                                  capture_output=True,text=True,timeout=.5,check=True)
            state=dict(line.split('=',1) for line in result.stdout.splitlines())
            if state.get('Active')!='yes' or state.get('LockedHint')!='no':
                raise RuntimeError('Desktop locked or inactive - PC keyboard released')
            self.next_seat_check=time.monotonic()+0.1
        for fd in self.fds:
            while True:
                try:data=os.read(fd,EVENT.size*64)
                except BlockingIOError:break
                if not data:raise RuntimeError('PC keyboard disconnected')
                if len(data)%EVENT.size:raise RuntimeError('Invalid keyboard event size')
                for _,_,kind,code,value in EVENT.iter_unpack(data):
                    if kind==0 and code==3:raise RuntimeError('Keyboard events dropped - PC mode released')
                    if kind==1 and value in (0,1,2):
                        # Both CASUE HID interfaces advertise every typing key.
                        # Grab both (so nothing leaks to Hyprland), but select
                        # one actual event source for typing during this mode.
                        if self.typing_fd is None and value==1 and code<=127:self.typing_fd=fd
                        if fd==self.typing_fd:self.queue+=self.layout.event(code,value)
                if len(self.queue)>256:raise RuntimeError('Keyboard queue full - check terminal input')

    def packet(self):
        n=min(32,len(self.queue))
        while n<len(self.queue) and n and self.queue[n]&0xc0==0x80:n-=1
        # Keep an arrow/control escape sequence in one packet.
        escape=self.queue.rfind(b'\x1b',max(0,n-5),n)
        if escape>0 and n<len(self.queue) and n-escape<6:n=escape
        data=self.queue[:n];self.queue=self.queue[n:];return data

    def close(self):
        for fd in self.fds:
            try:fcntl.ioctl(fd,GRAB,0)
            except OSError:pass
            os.close(fd)
        self.fds=[];self.queue=b'';self.typing_fd=None;self.next_seat_check=0
        if self.layout:self.layout.close();self.layout=None
