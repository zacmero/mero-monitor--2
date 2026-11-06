#!/usr/bin/env python3
"""Cross-check the actual device wire/auth code and host command lifecycle."""
import hashlib
import hmac
import json
from pathlib import Path
import subprocess
import tempfile
import threading
import time
import foston_agent as agent


def main():
    key=bytes(range(32))
    with tempfile.TemporaryDirectory() as directory:
        folder=Path(directory)
        exe=folder/"check-agent"
        subprocess.run(["cc","-std=c99","-Wall","-Wextra",str(agent.ROOT/"scripts/check_agent.c"),"-o",str(exe)],check=True)
        def c(mode,data,extra=()):
            return subprocess.check_output([str(exe),key.hex(),mode,*extra],input=data)
        for size in (0,1,55,56,63,64,65,127,4096):
            data=bytes(i%256 for i in range(size))
            expected=(hashlib.sha256(data).hexdigest()+"\n"+hmac.new(key,data,hashlib.sha256).hexdigest()+"\n").encode()
            assert c("hash",data)==expected,size
        fields={"v":1,"device":agent.DEVICE,"seq":1,"op":"status","path":""}
        signed=agent.sign(fields,key)
        assert c("verify",signed)==b"1\n"
        body=signed[:-69]
        assert c("sign",body)==signed
        assert agent.verify(c("sign",body),key)["op"]=="status"
        for bad in (signed[:-1],signed.replace(b"status",b"launch"),signed+b"x",signed.replace(b"\n",b"\r\n"),b"x"*2049):
            assert c("verify",bad)==b"0\n"
            try: agent.verify(bad,key);raise AssertionError("Bad signature accepted")
            except (ValueError,UnicodeError): pass
        for value,write,valid in [("/SDMMC/MERO/gallery.cfg",True,True),("/SDMMC/BT/proof.txt",True,True),("/SDMMC/MERO/../BOOT.exe",True,False),("/Windows/nk.exe",True,False),("/ResidentFlash/MERO/mero-shell.exe",True,False),("/SDMMC/MERO/agent-test/state",True,False),("/SDMMC/MERO/deploy-backups/old.exe",True,False),("/SDMMC/BT//bad.txt",True,False),("/Windows/coredll.dll",False,True),("/SDMMC",False,True),("/sdmmc",False,True)]:
            assert c("path",value.encode(),(str(int(write)),))==(b"1\n" if valid else b"0\n"),(value,write)
            try:agent.device_path(value,write);allowed=True
            except ValueError:allowed=False
            assert allowed==valid,(value,write)
        for number in (b"",b"-1",b"4294967296",b"1x"):assert c("number",number)==b"0\n"
        assert c("number",b"4294967295")==b"1\n"
        # Compile the actual transport loop against a provider that silently
        # truncates oversized writes, then also exercises partial I/O.
        source=(agent.ROOT/"wince/bluetooth/mero-bt.c").read_text()
        transfer=source[source.index("static int TransferBytes("):source.index("static unsigned int Exchange(")]
        shim=r'''
#include <assert.h>
#include <string.h>
#define WSAEWOULDBLOCK 10035
static int channel,used,calls;
static unsigned char received[4096];
static int WaitSocket(int reading){(void)reading;return 1;}
static int WSAGetLastError(void){return WSAEWOULDBLOCK;}
static int send(int fd,const char *p,unsigned int n,int flags){
 (void)fd;(void)flags;if(!calls++)return -1;
 if(n>256){memcpy(received+used,p,256);used+=256;return n;}
 unsigned int take=n>73?73:n;memcpy(received+used,p,take);used+=take;return take;
}
static int recv(int fd,char *p,unsigned int n,int flags){(void)fd;(void)p;(void)n;(void)flags;return -1;}
'''
        main_c=r'''
int main(void){unsigned char data[4096];unsigned int i;for(i=0;i<sizeof(data);i++)data[i]=(unsigned char)i;
assert(TransferBytes(data,sizeof(data),0));assert(used==sizeof(data));assert(!memcmp(data,received,sizeof(data)));return 0;}
'''
        harness=folder/"transport.c";harness.write_text(shim+transfer+main_c)
        subprocess.run(["cc","-std=c99","-Wall","-Wextra",str(harness),"-o",str(folder/"transport")],check=True)
        subprocess.run([str(folder/"transport")],check=True)
        client=agent.Client(folder/"state",folder/"share",0.1)
        agent.atomic(client.state/"key",key.hex().encode())
        try:client.rpc("status");raise AssertionError("Offline device reported success")
        except RuntimeError as e:assert "No authenticated reply" in str(e)
        assert (client.state/"pending.json").exists()
        try:client.rpc("status");raise AssertionError("Overwrote unresolved job")
        except RuntimeError as e:assert "Previous job unresolved" in str(e)
        pending=json.loads((client.state/"pending.json").read_text())
        wrong=agent.sign({"v":1,"device":agent.DEVICE,"seq":999,"op":"status","ok":1},key)
        agent.atomic(client.share/"agent-result.txt",wrong)
        try:client.wait();raise AssertionError("Accepted stale result")
        except RuntimeError:pass
        result=agent.sign({"v":1,"device":agent.DEVICE,"seq":pending["seq"],"op":"status","ok":1,"message":"agent_ready"},key)
        agent.atomic(client.share/"agent-result.txt",result)
        assert client.wait()["message"]=="agent_ready"
        assert not (client.state/"pending.json").exists()
        data=b"verified device config\n"
        def respond():
            for _ in range(100):
                try:
                    job=agent.verify((client.share/"agent-job.txt").read_bytes(),key)
                    if job["op"]=="get":break
                except (OSError,ValueError):pass
                time.sleep(.01)
            agent.atomic(client.share/"agent-output.bin",data)
            agent.atomic(client.share/"agent-result.txt",agent.sign({"v":1,"device":agent.DEVICE,"seq":job["seq"],"op":"get","ok":1,"size":len(data),"sha256":hashlib.sha256(data).hexdigest()},key))
        t=threading.Thread(target=respond);t.start();client.timeout=3
        out=folder/"result.txt"
        assert client.rpc("get","\\SDMMC\\BT\\proof.txt",output=out)["local_file"]==str(out)
        t.join();assert out.read_bytes()==data
        client.timeout=.05
        try:client.rpc("status");raise AssertionError("Offline device reported success")
        except RuntimeError:pass
        assert client.cancel_read()["cancelled_read"]
        try:client.rpc("put","\\SDMMC\\BT\\config.txt",payload=b"config");raise AssertionError("Offline mutation reported success")
        except RuntimeError:pass
        try:client.cancel_read();raise AssertionError("Cancelled an uncertain mutation")
        except RuntimeError as error:assert "Cannot cancel" in str(error)
        assert (client.state/"pending.json").exists()
    print("PASS: C/Python SHA/HMAC, path bounds, stale/offline jobs, verified retrieval, bounded partial writes, read-only cancellation")


if __name__=="__main__":main()
