#!/usr/bin/env python3
"""Offline checks for authentication, targeting, stale controls and deduplication."""
import base64
import json
from pathlib import Path
import tempfile
import time
from unittest.mock import patch
import companion_live as live


def check():
    key=bytes(range(32))
    state=dict(player="firefox.instance_1",track="abcdefghijk",trackid="/firefox",
               title="Á song|with newline\n",artist="Artist",status="PLAYING",pos=31,length=200,
               next=True,previous=True,playpause=True,seek=True)
    # Test a real GetAll-shaped reply, including timing unavailable at first load.
    meta={"xesam:url":{"data":"https://music.youtube.com/watch?v=abcdefghijk"},
          "xesam:title":{"data":"Song"},"xesam:artist":{"data":["Artist"]},
          "mpris:length":{"data":200000000}}
    properties={"Metadata":{"data":meta},"Position":{"data":31000000},"PlaybackStatus":{"data":"Playing"}}
    with patch.object(live,"run",side_effect=lambda _:json.dumps({"data":[properties]})):
        assert live.player_state("firefox.instance_1")["pos"]==31
        assert live.player_state("firefox.instance_1")["length"]==200
        meta["xesam:url"]["data"]="https://www.youtube.com/watch?v=abcdefghijk"
        assert live.player_state("firefox.instance_1") is None
        meta["xesam:url"]["data"]="https://music.youtube.com.evil.test/watch?v=abcdefghijk"
        assert live.player_state("firefox.instance_1") is None
    with tempfile.TemporaryDirectory() as directory:
        folder=Path(directory);share=folder/"share";private=folder/"private";private.mkdir();share.mkdir()
        (private/"key").write_text(key.hex()+"\n")
        feed=live.BluetoothLive(share,private)
        feed.publish(["MERO:POWER:standby=0",live.music_line(state)])
        raw=(share/"live-state.txt").read_bytes()
        fields=live.verified(raw,key)
        assert "Á song/with newline " in base64.b64decode(fields["data"]).decode()
        try:live.verified(raw.replace(b"epoch=",b"wrong="),key);raise AssertionError("accepted modified snapshot")
        except ValueError:pass
        def event(identifier,**changes):
            fields=dict(v=1,device=live.DEVICE,id=identifier,created=int(time.time()),action="next",track=state["track"],value=0)
            fields.update(changes);(share/"live-event.txt").write_bytes(live.signed(fields,key))
        with patch.object(live,"music_state",return_value=state),patch.object(live,"run",return_value="") as command:
            event("1-123-100");feed.publish([]);assert command.call_count==1
            feed.publish([]);assert command.call_count==1
            # A host restart must not execute an acknowledged button again.
            feed=live.BluetoothLive(share,private);feed.publish([]);assert command.call_count==1
            event("1-123-101",track="othertrack1");feed.publish([])
            assert feed.result=="track_changed_press_again" and command.call_count==1
            event("1-123-102",created=int(time.time())-60);feed.publish([])
            assert feed.result=="expired_press_again" and command.call_count==1
            event("1-123-103",action="seek",value=42);feed.publish([])
            assert command.call_args.args[0][-3:]==["ox","/firefox","42000000"]
            assert feed.result=="ok" and command.call_count==2
            event("1-123-104",action="seek",value=100000);feed.publish([])
            assert feed.result=="control_failed_press_again" and command.call_count==2
            event("1-123-105",action="shell");feed.publish([])
            assert feed.result=="control_unavailable" and command.call_count==2
    print("PASS: signed live snapshots, exact YouTube Music targeting/timing, stale/changed-track rejection, durable command deduplication")

if __name__=="__main__":check()
