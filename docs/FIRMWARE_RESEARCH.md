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
2. Evaluate a documented `RegSaveKey` export of the live system/user hives to owned SD files, then retrieve and hash them. No import/replace operation is authorized by this investigation.
3. Obtain matching CE storage-manager headers before any documented store/partition enumeration. The current SDK lacks the relevant headers; do not guess structure layouts or raw disk calls.
4. Design a timed ScreenOff/On experiment with an independent recovery path before trying it. An accepted power-state request alone cannot verify physical LCD darkness. Whole-device suspend may lose the Bluetooth control path.
5. Establish exact board/flash layout and a known restoration route before considering any persistent startup or firmware change. No auto-power-on fix has been established.

Primary API references: [GetDevicePower](https://learn.microsoft.com/en-us/previous-versions/windows/embedded/aa447831%28v%3Dmsdn.10%29), [SetSystemPowerState](https://learn.microsoft.com/en-us/previous-versions/windows/embedded/ms920754%28v%3Dmsdn.10%29), [FindFirstStore](https://learn.microsoft.com/en-us/previous-versions/windows/embedded/ms889696%28v%3Dmsdn.10%29). These describe CE APIs, not guaranteed behavior of this OEM implementation.

Validation: helper build, report process identity and signed retrieval passed; `check_firmware_probe.py` checks the actual bounded report writer and rejects prohibited API references. Full repository checks passed. These checks support the investigation's limited behavior; they do not certify flash recovery.
