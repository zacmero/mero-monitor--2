# Foston firmware research — 2026-11-07

Initial investigation is read-only: backup audit, selected registry/filesystem queries and offline inspection of copied OEM executables. No flashing, raw flash access, persistent registry edits, power-policy changes or guessed device controls were used. Ambient remained available in front of the agent.

## Backup coverage

We do **not** have a complete firmware/bootloader image or a verified firmware restoration procedure.

- Repository `backup/`: nine files, 72,306 bytes, mainly factory reference/configuration and a file listing.
- Private OEM collection: 117 files, 16,097,872 bytes. The existing 64-entry SHA256 manifest matched every referenced file with no missing files or mismatches. These are file copies, not a flash image.
- A second local archive preserves all 117 files at `~/.local/state/mero/firmware-backups/20261107/oem-file-copy-NOT-full-firmware.tar.gz`. Archive size6,456,471 bytes; SHA256 `4decc711641a788f03c7d0db30a5651a3cd28c5f55c607dcc0c12844f923aa7c`. Every archived file was compared against its source hash. This is on the same PC, so it is not an independent off-machine backup.
- Successfully retrieved the factory `\Windows\default.hv`: 323,584 bytes, SHA256 `767217403fdb17742188adba5a8737ac953bd679e99f54cb9153e87f7b745c06`, agent job460. This is a default registry hive, not the live registry or an OS image. Raw strings suggest the factory launcher was `launch.exe`; structured hive parsing is still required before treating that as a decoded registry value.
- Device `SDMMC/BACKUP` enumeration returned no files. ResidentFlash2 `ARC.BIN` and `BEE.BIN` identities remain unknown. SD logo files have screen-sized payloads; their names do not establish that they contain firmware.

Evidence and hash manifests are private under `~/.local/state/mero/foston-evidence/20261107-firmware/`. OEM binaries, hives and private device identifiers should remain outside Git.

## Verified live configuration

The optional native helper `wince/probe/mero-firmware-probe.c` reads registry keys, lists bounded directories, queries documented power state and reads small regular-file headers. Its only writes are its own bounded report and preservation of an earlier report under SDMMC/MERO. It does not open raw storage devices or issue IOCTLs.

Latest helper: 23,040 bytes, SHA256 `1dd9a27856312378bd395b69fd4a5cd40ce738c0721dda0376026446e1306be7`, deployment456/launch457. Retrieved report458: 25,128 bytes, SHA256 `02f8a571b44b9515acc949aa6f3b8ba5586bdf44fc51ff231ee20d0592520f95`. It visited121 selected keys without truncation. This is a targeted inventory, not a complete registry backup.

- Windows CE5.0 build1400; reported physical memory55,902,208 bytes.
- Live Launch50 remains `\ResidentFlash\MERO\mero-shell.exe`. Other observed startup entries include device.exe, gwes.exe, services.exe and btpower.exe. None was changed.
- StorageManager declares BINFS, HiveDisk, ResidentFlash2 and ResidentFlash profiles. Active DSK9 refers to BuiltIn/MSFLASH; DSK3 refers to the SD high-capacity driver. These registry profiles do not establish physical partition offsets, NAND geometry or bootloader placement.
- The SPC driver configuration includes `ChargerInWakeup=1`, `ChargerOutWakeup=1`, `Suspend=1`, `Unattended=1` and `PowerOff=0`. This is evidence of an OS charger/wake policy, not proof that changing it would enable cold boot. Charging-only startup cannot yet be assigned exclusively to the bootloader.
- The Power Manager has a named `ScreenOff` state with display-class D4 while its default is D0. This differs from requesting D4 directly on BKL1. It is a research candidate; no ScreenOff transition was attempted. Suspend configuration powers down the network class, so Bluetooth wake must not be assumed.
- Current system power query returned `on`; cached and forced BKL power queries returned D0 while awake. Neither observation proves physical backlight-off behavior.

## Offline OEM findings

The archived Gui.dll hash matches the inspected OEM copy. Its brightness routines implement the already verified BKL1 GET/SET and raw range14–64 in steps of5; the user confirmed the minimum remains bright.

Gui.dll's `CBatCtrl::SetPWState` export stores an integer in a global variable; its name does not establish a hardware power transition. PowerDown.exe and jwdmain.exe reference power-key messages and GUI power handling. JWDConfigUpdate.exe contains configuration strings, but was not executed and has not been established as a firmware flasher.

Stream reads of ROM modules such as buses.dll failed. No bypass was attempted. The default registry hive was an ordinary supported file read and succeeded.

## Next evidence-backed steps

1. Parse the copied default hive offline using a reviewed CE hive parser, keeping decoded contents private. Compare factory startup/power values with the live inventory.
2. Investigate live hive export semantics before executing an export. `RegSaveKey` can invoke an OEM persistence hook; its name does not make it a read-only operation. No import/replace operation is authorized by this investigation.
3. Obtain matching CE storage-manager headers before any documented store/partition enumeration. The current SDK lacks the relevant headers; do not guess structure layouts or raw disk calls.
4. Design a timed ScreenOff/On experiment with an independent recovery path before trying it. An accepted power-state request alone cannot verify physical LCD darkness. Whole-device suspend may lose the Bluetooth control path.
5. Establish exact board/flash layout and a known restoration route before considering any persistent startup or firmware change. No auto-power-on fix has been established.

Primary API references: [GetDevicePower](https://learn.microsoft.com/en-us/previous-versions/windows/embedded/aa447831%28v%3Dmsdn.10%29), [SetSystemPowerState](https://learn.microsoft.com/en-us/previous-versions/windows/embedded/ms920754%28v%3Dmsdn.10%29), [FindFirstStore](https://learn.microsoft.com/en-us/previous-versions/windows/embedded/ms889696%28v%3Dmsdn.10%29). These describe CE APIs, not guaranteed behavior of this OEM implementation.

Validation: helper build, report process identity and signed retrieval passed; `check_firmware_probe.py` checks the actual bounded report writer and rejects prohibited API references. Full repository checks passed. These checks support the investigation's limited behavior; they do not certify flash recovery.


## Extended live map — 2026-11-07

The new headless probe completed with 134 keys, no truncation and a healthy
writer. Signed GET 781 retrieved 34,421 bytes, SHA-256
`5cb498e10971c37c8cff1097e45674d78b9413e537800a1f6760b55eef29ce5e`.
Probe deployment 778 preserved backup 778; executable read-back matched
`1aceed706720a869c2e504695036d7cc884d05ac01b2a969927fa4b2a548845f`
(27,648 bytes). Launch 780 produced this report. No hardware setters, privileged
memory APIs, registry writes, flash operations or power transitions were invoked.

| Area | Evidence and practical control boundary |
|---|---|
| Platform | OEM `MTK`, platform `APOLLO`, ARM architecture 5 / processor level 4 / revision 4. Exact SoC and board revision remain unidentified. |
| Low-level API surface | `KernelIoControl`, `VirtualCopy`, `SetKMode`, `SetProcPermissions`, `ReadProcessMemory` and `MapCallerPtr` are exported by coredll. Addresses were resolved only; privileges, mappings and hardware access were not tested. |
| Driver map | 35 BuiltIn entries correlated with active stream names; 92 loaded modules enumerated. The system-wide list includes buses, DMA, SPC, FMC, USB function, GPS, NAND block and display modules. A DLL absent from this probe process can still be loaded elsewhere. |
| GPIO / DMA | Registry GPIO `buses.dll`, GIO1, MemBase `0x80002000`; DMA `dmadrv.dll`, `0x80023000`. These are configured addresses, not validated physical addresses or permission to access registers. |
| Display / PWM | `ddi.dll`, BKL/PWM interfaces and existing OEM brightness control. Minimum brightness works; actual backlight-off and reliable remote wake remain unverified. ScreenOff is configured to put the display class in D4 while leaving the default in D0. |
| FM | FMC driver is loaded. Live-only values report Power=1, Enable=1, FreqKHz=107900, Frequency=1079. They do not prove RF activity, an IOCTL contract, or a safe way to change transmission. |
| Bluetooth | MTK UART plus ANW/Windows Bluetooth modules are loaded; our signed two-way agent operation is verified. No adapter/driver replacement is indicated by this map. |
| Storage | NAND block/profile and three configured partitions are visible. There is still no raw NAND image, bootloader dump, exact geometry or tested restoration route. |
| Object store | Capacity 3,678,208 bytes; free 551,180 bytes during this probe. Physical free RAM was 40,443,904 bytes. This explains why free RAM does not make Temp staging reliable; retain the RAM-only transfer path. |
| Power-on | Charger wake flags exist in SPC configuration. They concern the running OS; they do not establish a charging-screen/bootloader auto-start switch. |

The factory hive was decoded offline with [hvtool](https://github.com/nlitsme/hvtool),
commit `7093a5c125bdc0d78d23adf9afe6801a8ae25b1d`, using itslib commit
`49f88ee0f292f24bbc7a32d2cbdeb8821d40ad3d`. A private portable build substituted
read-only FileReader for MmapReader; parsing logic was unchanged. The source
hive SHA-256 remained `767217403fdb17742188adba5a8737ac953bd679e99f54cb9153e87f7b745c06`.
All 1,084 decoded sections stay private. Factory Launch50 is confirmed as
`launch.exe`; the live resident recovery-shell path is preserved. The comparison
found eight differences among comparable values in shared queried sections,
including calibration, startup, display metadata and four live FM values. This
is a bounded comparison, not a complete live-registry diff. Default (`@`) values
are normalized explicitly; unknown binary encodings are excluded from comparison.

Private evidence: `~/.local/state/mero/foston-evidence/firmware-map-20261107-225234/`.
`map-v2.json` is the corrected derived map; `map.json` is retained as an earlier
parser result that omitted factory unnamed values. Use `scripts/foston_firmware_map.py`
to derive future comparisons; it rejects incomplete reports and refuses output overwrite.

[RegSaveKey](https://learn.microsoft.com/en-us/previous-versions/windows/embedded/ms891466(v=msdn.10))
is exported, but was deliberately not called: the documented
[OEM hive persistence hook](https://learn.microsoft.com/en-us/previous-versions/windows/embedded/ms904469(v=msdn.10))
can copy the system hive to persistent storage. Matching CE storage-manager
headers are still required before store/partition enumeration. The strongest
next steps are offline OEM driver-contract analysis and documented read-only
storage geometry enumeration, followed by establishing a restoration route.
Direct register writes or boot experiments are not justified by export presence.
