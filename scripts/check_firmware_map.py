#!/usr/bin/env python3
"""Exercise real firmware evidence parsing, comparison and incomplete rejection."""
from foston_firmware_map import build_map,registry,value
live=r"""[HKLM\Drivers\BuiltIn\GPIO]
Dll=buses.dll
Prefix=GIO
MemBase=2147491840 (0x80002000)
[HKLM\Drivers\Active\01]
Key=Drivers\BuiltIn\GPIO
Name=GIO1:
[HKLM\init]
Launch50=\ResidentFlash\MERO\mero-shell.exe
[LOADED EXPORT PRESENCE ONLY - NOT EXECUTED]
export=coredll.dll!VirtualCopy present=1 address=0x12340000
module=buses.dll base=0x01000000 bytes=8192 path=\Windows\buses.dll
COMPLETE keys=3 bytes_before_footer=400 truncated=0 healthy=1
"""
factory=r"""REGEDIT4
[HKEY_LOCAL_MACHINE\Drivers\BuiltIn\GPIO]
 "Dll"="buses.dll"
 "Prefix"="GIO"
 "MemBase"=dword:80002000
[HKLM\init]
 "Launch50"="launch.exe"
"""
m=build_map(live,factory)
assert len(m['drivers'])==1 and m['drivers'][0]['active_names']==['GIO1:']
assert m['drivers'][0]['configuration']['membase']==0x80002000
assert m['changes']==[dict(key='hklm\\init',name='launch50',factory='launch.exe',live='\\ResidentFlash\\MERO\\mero-shell.exe')]
assert m['exports'][0]['present'] and m['modules'][0]['virtual_base']=='0x01000000'
assert value('multi_sz:"80060000", "80001000"',True)==['80060000','80001000']
assert value('80060000|80001000||',False)==['80060000','80001000']
assert isinstance(value('hex:01,02',True),dict)
assert registry('[HKLM\\Example]\n @=dword:00000004',True)['hklm\\example']['default']==4
for bad in (live.replace('truncated=0','truncated=1'),live.replace('healthy=1','healthy=0'),live.split('COMPLETE')[0]):
 try:build_map(bad,factory)
 except ValueError:pass
 else:raise AssertionError('incomplete probe accepted')
try:value('1 (0x00000002)',False)
except ValueError:pass
else:raise AssertionError('inconsistent DWORD accepted')
print('PASS: firmware driver/active-name map, factory comparison, value types and incomplete-evidence rejection')
