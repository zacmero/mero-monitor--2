# Factory Reference & Original System Backup

Minimal reference backup of the Foston FS-460BT original SD card configuration.

---

## 1. Preserved Configuration Files

* `DEVICE.NNG`: Device license identifier token for vendor navigation packages.
* `IGO8/SYS.TXT`: Primary iGO8 configuration (defines 480×272 landscape display and `COM1:` 9600 baud GPS).
* `Drive/*.ini` & `*.conf`: Sygic DRIVE platform configurations (defines RGB565 color mode and `COM1:` 9600 baud serial port).
* `Res/countriesinfo.xml`: Vendor resource schema definition.
* `ORIGINAL_FILE_MANIFEST.txt`: Complete recursive listing of every directory, file, size, and permission present on the original factory card.

---

## 2. Factory Navigation Paths

* **iGO 8**: `\SDMMC\IGO8\iGO8.exe`
* **Sygic DRIVE**: `\SDMMC\Drive\WindowsCE\Drive.exe`

These original applications can still be launched directly from the Custom Mero Shell menu at any time.
