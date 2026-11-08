#!/usr/bin/env python3
"""Compare read-only probe data with a privately decoded CE factory hive.

Writes a derived JSON map only. Addresses are registry/module metadata, never
memory-read/write requests. Inputs are data; unknown value encodings stay raw.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re


def key_path(path):
    roots={"hkey_local_machine":"hklm","hkey_current_user":"hkcu","hkey_classes_root":"hkcr"}
    parts=path.casefold().split("\\")
    parts[0]=roots.get(parts[0],parts[0])
    return "\\".join(parts)


def value(text,factory):
    if factory:
        if re.fullmatch(r'dword:[0-9a-fA-F]{8}',text):return int(text[6:],16)
        if text.startswith('multi_sz:'):
            try:return json.loads('['+text[9:]+']')
            except ValueError:pass
        if text.startswith('"'):
            try:return json.loads(text)
            except ValueError:pass
        return {"unparsed":text}
    number=re.fullmatch(r'([0-9]+) \(0x([0-9a-fA-F]{8})\)',text)
    if number:
        if int(number[1])!=int(number[2],16):raise ValueError('inconsistent DWORD in probe')
        return int(number[1])
    if text.startswith('[type='):return {"unparsed":text}
    if text.endswith('||'):return text.rstrip('|').split('|')
    return text


def registry(text,factory=False):
    sections={};current=None
    for raw in text.splitlines():
        line=raw.strip()
        if line.startswith('[') and line.endswith(']'):
            path=key_path(line[1:-1])
            current=sections.setdefault(path,{}) if path.startswith(('hklm\\','hkcu\\','hkcr\\')) else None
        elif current is not None:
            if factory and line.startswith("@="):
                current["default"]=value(line[2:],True)
                continue
            match=re.fullmatch(r'"((?:\\.|[^"\\])*)"=(.*)',line) if factory else re.fullmatch(r'([^=]+)=(.*)',line)
            if match and match[1]!='open_error':current[match[1].casefold()]=value(match[2],factory)
    return sections


def build_map(probe,factory):
    footer=re.search(r'^COMPLETE keys=(\d+) bytes_before_footer=(\d+) truncated=(\d+) healthy=(\d+)$',probe,re.M)
    if not footer or footer[3]!='0' or footer[4]!='1':raise ValueError('probe is missing a healthy, untruncated completion footer')
    live=registry(probe);default=registry(factory,True)
    active={}
    for path,data in live.items():
        if path.startswith('hklm\\drivers\\active\\') and isinstance(data.get('key'),str) and data.get('name'):
            target=key_path(data['key'])
            if not target.startswith('hklm\\'):target='hklm\\'+target
            active.setdefault(target,[]).append(data['name'])
    drivers=[];base='hklm\\drivers\\builtin'
    for path,data in live.items():
        if path==base or (path.startswith(base+'\\') and '\\' not in path[len(base)+1:]):
            if 'dll' not in data:continue
            drivers.append(dict(key=path,active_names=active.get(path,[]),configuration=data,
                                factory_present=path in default))
    changes=[]
    for path,data in live.items():
        if path not in default:continue
        for name,current in data.items():
            old=default[path].get(name)
            if isinstance(old,dict) or isinstance(current,dict):continue
            if old!=current:changes.append(dict(key=path,name=name,factory=old,live=current))
    exports=[];modules=[]
    for line in probe.splitlines():
        match=re.fullmatch(r'export=([^!]+)!([^ ]+) present=([01]) address=(0x[0-9a-fA-F]+)',line)
        if match:exports.append(dict(module=match[1],symbol=match[2],present=match[3]=='1',address=match[4]))
        match=re.fullmatch(r'module=(\S+) base=(0x[0-9a-fA-F]+) bytes=(\d+) path=(.*)',line)
        if match:modules.append(dict(module=match[1],virtual_base=match[2],bytes=int(match[3]),path=match[4]))
    return dict(meaning='Read-only metadata; export presence is not tested privilege and MemBase is not proven physical address',
                probe_keys=int(footer[1]),factory_keys=len(default),drivers=drivers,changes=changes,exports=exports,modules=modules)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('probe',type=Path);parser.add_argument('factory_reg',type=Path);parser.add_argument('output',type=Path)
    args=parser.parse_args()
    # Universal newlines normalize CRLF without modifying original evidence.
    result=build_map(args.probe.read_text(),args.factory_reg.read_text())
    result['input_sha256']={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in (args.probe,args.factory_reg)}
    with args.output.open('x') as output:json.dump(result,output,indent=2);output.write('\n')
    print(json.dumps({name:len(result[name]) for name in ('drivers','changes','exports','modules')}))


if __name__=='__main__':main()
