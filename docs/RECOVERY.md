# Foston preservation and recovery

## Current position

The working device is valuable. Preservation takes precedence over firmware
experiments. This mission changes no device executable, startup configuration,
registry setting, power state or firmware. Signed inventory/download commands
still advance the normal agent journals; this is not a zero-write forensic image.

A file backup can recover a damaged custom app when an execution/transfer route
survives. It cannot recover an erased bootloader or guarantee recovery from a
non-booting Windows CE system. There is currently **no full firmware image and
no tested factory restoration procedure**. Do not flash this device.

## Recovery routes and limits

| Failure | Route | Evidence / limit |
|---|---|---|
| Custom app misbehaves; agent answers | Stop that named custom app, restore its verified prior executable/config, launch the shell home | Signed stop/put/get/rollback/launch have been exercised previously. Not every app's failure mode has been tested. |
| SD shell is unavailable | Resident shell is the existing Launch50 target; its chainloader attempts the SD shell | Resident file is preserved. Chainloader source handles missing/unlaunchable SD shell. A shell that starts then crashes is not covered by that check. No failure injected in this mission. |
| Agent stopped, device UI works | Reopen the existing keyed navigation executable or use the shell's provisioned Bluetooth connection action | Existing navigation path and successful agent bootstrap are known. Requires physical access and usable UI. |
| Bluetooth unavailable | Check host services, power, USB mode and the running keyed agent; use existing vendor UI only if needed | USB has been unreliable on mero-machine. Mass Storage is not an independent dependable rescue path here. |
| Bad SD card / shell crash before UI | Potential vendor navigation selector, known-good alternate SD or recovery from another PC | Conditional and not rehearsed. User has no direct SD reader currently. Do not remove the SD or alter navigation while the device is useful. |
| CE/bootloader fails or flash layout damaged | No established recovery route | Stop. File copies and similarly named firmware packages do not establish compatibility. |

The preserved resident shell is older than the current SD shell. Preserve that
working fallback; do not synchronize resident flash as routine deployment.
Existing resident images are not evidence that their boot registry was backed up.

## Working bootstrap chain

- Boot Launch50 currently points to `\ResidentFlash\MERO\mero-shell.exe`.
- Resident shell contains a handoff to `\SDMMC\MERO\mero-shell.exe`.
- The SD shell reads `shell.cfg` for the default app and `agent.cfg` for the keyed
  agent executable. A shell-home launch suppresses default-app entry.
- The installed agent path is `\SDMMC\BT\Downloads\101907-mero-agent.exe`.
  Do not substitute the plain `mero-bt.exe` transfer client.
- USB must remain ActiveSync / PC Link. Mass Storage can reserve the SD card and
  cause Bluetooth writes to report read-only memory.
- Device address: `00:18:E4:1F:03:AF`. Paired host adapter:
  `8C:86:DD:83:27:70`. Changing host or adapter is a separate provisioning task.
- There is no working battery. Keep power connected. Restored power enters the
  charging screen; physical power-button operation is still needed to boot.

## Preservation tools

Run from the repository on the provisioned host. These commands collect signed
reads; they do not deploy, stop apps, reboot or change boot settings.

```bash
rtk proxy python3 scripts/foston_preserve.py snapshot /absolute/private/NEW-directory
rtk proxy python3 scripts/foston_preserve.py verify /absolute/private/NEW-directory
```

The destination must not exist. Collection covers top-level files in SDMMC/MERO,
ResidentFlash/MERO, SDMMC/BT and BT/Downloads plus SD shell.ini. It includes the
exact running keyed agent, signed receipts, private host agent state and selected
host configuration/service definitions. It does not recursively copy the SD,
navigation maps, deployment backup directories, or device agent journals. Host
state is copied under the existing client lock after a successful final status,
with no unresolved job. Dynamic logs/configs remain point-in-time reads, not an
atomic filesystem snapshot. The executable identity is checked against status.

Output is private (directories 0700, files 0600). The manifest verifies every
included file's size and SHA256 and rejects missing/extra files and symlinks.
It detects accidental corruption, not tampering by someone who can replace both
data and manifest. Keep the archive checksum separately with the recovery record.

On timeout, retain the incomplete directory and reconcile the pending job:

```bash
rtk proxy python3 scripts/foston_agent.py --timeout 120 wait
```

Do not reset counters, delete pending state or retry an uncertain mutation.
After reconciliation, a new snapshot uses a new directory. Only unresolved read
jobs may be abandoned with `cancel-read`, after checking the actual situation.

## Recovering one custom app while the agent works

1. Keep power connected and USB in PC Link. Record the symptom and current build.
2. Query agent status. If it fails, reconcile any pending job before doing more.
3. Identify the exact known-good executable/config and its signed deployment
   receipt or verified recovery manifest. Do not choose a backup by filename alone.
4. Stop only the affected `mero-*.exe` app. Never stop the agent, Bluetooth stack,
   PhoneLink or CE processes.
5. If a verified deployment backup exists, use `rollback JOB_NUMBER`. Otherwise
   use `put` for the selected saved file under SDMMC/MERO. Preserve the returned
   backup reference. Do not replace the running agent this way.
6. Retrieve the restored file and compare its SHA256 with the intended copy.
7. Launch the shell with `launch mero-shell.exe --home`, then the restored app.
8. Verify device behavior visually and check agent status. A launch reply alone
   does not establish recovery. Preserve the failed file and receipts for analysis.

Agent self-replacement must use `foston_agent.py upgrade`, which stages, hashes,
backs up and verifies the replacement. This guide does not authorize modifying
upgrade plans or copying an old keyed executable over the running agent.

## If agent status fails

Check the existing host services first:

```bash
rtk proxy systemctl --user status mero-bt-ftp.service mero-bridge.service
rtk proxy python3 scripts/foston_agent.py --timeout 120 wait
```

If there is no pending job, use `status` instead of `wait`. If the service is down,
inspect its log and restart that service; do not remove pairing or change adapters.
If the device is awake, reopen its configured navigation app or the shell's
Bluetooth connection action. Confirm AGENT ON, then obtain an authenticated reply.
Do not repeatedly reset the device to troubleshoot a host-side problem.

If the device cannot reach the custom shell but the vendor UI is usable, the
known keyed navigation path above is the existing bootstrap route. Select it
only after confirming that exact file is still present. A proof-file download
through the vendor Bluetooth FTP client worked previously in PC Link mode.
Do not overwrite the only launcher or change Launch50 to test recovery.

## Credentials and independent copies

The bundle contains the agent key, keyed executables, counter, receipts and user
configuration. Keep it outside Git and public FTP shares. Preserve the OEM file
collection, decoded hive/evidence and obex compatibility runtime alongside it.
A source Git bundle preserves tracked code and history; generated keyed builds
and private runtime files need the private bundle as well.

An old counter snapshot is **not** safe to restore over a newer active host/device.
Never reset sequence numbers or roll device journals back to match a backup.
A lost-host recovery must reconcile the device's durable sequence and key through
a separately reviewed procedure. Linux Bluetooth pairing keys may need root access
and are not included by the unprivileged collector. The adapter dependency also
remains. Having the agent key alone does not establish new-host recovery.

An encrypted copy also needs an independently preserved recovery key. Keeping
that key only on the original Linux disk does not protect against losing that
disk. Do not post the key in chat, Git or the Bluetooth share.

Maintain a copy on another physical device, preferably off-machine. On filesystems
that cannot enforce private permissions, encrypt the bundle before copying it.
No independent copy is complete until its checksum and offline manifest verify.

## Future device lessons

Before modifying another device:

1. Record the exact model/board/revision, power behavior, storage and recovery entry
   points while factory software still works.
2. Acquire a genuine matching firmware/recovery package and establish what it
   actually restores; avoid similar-model firmware.
3. Back up readable files, boot settings, credentials and tool versions. Label file
   copies, partition images and full flash dumps accurately.
4. Store a hash-verified copy off the device and off the development machine.
5. Keep an unchanged recovery launcher and a control channel independent of the
   app being developed. Check which failures that channel cannot survive.
6. Make one scoped change at a time. Back up, deploy, read back and verify behavior.
7. Design recovery tests with an independent route before intentionally breaking
   anything. A backup is not a tested restoration procedure.

Next for this Foston: investigate exact OEM recovery contracts offline, obtain
matching CE storage enumeration headers, and map storage geometry through
read-only APIs. No raw disk writes, guessed IOCTLs, bootloader edits, whole-device
suspend or firmware experiments are part of this preservation mission.

## Preserved working state — 2026-11-07

Collection completed with final signed status **895**, `agent_ready`. The running
agent SHA256 matched its downloaded executable. No apps, boot configuration or
power state were changed. The collector downloaded **100 device files**; the
enriched private manifest covers **1681 files** plus the manifest itself.

The encrypted bundle is available in the explicitly ignored repository directory
`recovery-private/20261107-foston-preservation-v2.tar.gz.gpg`. A byte-identical copy
is on `/mnt/windows-ssd/Mero-Recovery/Foston/`, a different physical drive
(`/dev/sda3`; Linux root is `/dev/sdc1`). This is independent of the Linux disk,
but still in the same computer. No off-machine copy has been established.

All 1682 archived file members were hash-compared with the sealed source set.
GnuPG decryption reproduced the original archive hash. The second-drive and
repository encrypted copies matched:

`0d9c07f34604fe569dc23c01894f9efdd3514081f2ab1cb14d823d047c6d0f5e`

Plain archive SHA256:
`af07b9d322df4da298cb0d5e3d38befa8994c0e959dc1db9b153b050e2ea6e9a`.
The plaintext archive is 33,734,820 bytes. Private snapshot and detailed receipts:
`~/.local/state/mero/foston-recovery/20261107-working-device/`.

The encryption passphrase is outside Git at
`~/.config/mero/foston-recovery/20261107-archive-passphrase`. The user confirmed on 2026-11-08 that the passphrase was independently saved.
That location is user-managed and has not been inspected by the agent. Keep it
private and outside this Linux disk. Its value is not printed in agent output or committed.

The archive includes source history at tooling commit `2af58dc`, installed SD
and resident apps/configuration, the exact keyed agent, private host agent state,
OEM file-copy archive, decoded factory-hive evidence, obex runtime, keyboard
udev rule and terminal shortcut wrapper. Unrelated host secret files are excluded.
Neither Linux pairing secrets nor device agent journals/full navigation maps
are included. This verifies backup integrity and decryption, **not** a boot or
firmware restoration procedure. No intentional failure/reboot test was run.

## Display milestone recovery increment — 2026-11-08

After commit `de2954c`, an encrypted increment preserves the updated agent,
verified app read-backs, source history, signed receipts, current host agent
state and screen evidence. It complements the baseline; it is not a complete
replacement for it. Both the repository copy and second-drive mirror were
hash-verified, archive members checked and decryption tested with the same
passphrase as the baseline.

- Repository: `recovery-private/20261108-display-increment.tar.gz.gpg`.
- Mirror: `/mnt/windows-ssd/Mero-Recovery/Foston/20261108-display-increment.tar.gz.gpg`.
- Cipher SHA256: `a7e721e46430e2f55df6381fcf94b0b326f93850a543da7749df0babaacc9f67`.
- Manifest:1478 files;1479 archived file members including the manifest.
- App backups:906 terminal,909 Ambient. Agent old backup:
  `\SDMMC\BT\AgentUpdate\old-d84a0019019b8f7a.exe`.

Ambient photo mapping changed from V1 to V2. A rollback to the older Ambient
renderer requires a compatible agent build through the signed upgrade workflow.
Do not directly overwrite the running agent or assume one old app file reverses
a protocol change. The preserved source commits and keyed versions support a
reviewed coordinated rollback; no destructive rollback test was run.
