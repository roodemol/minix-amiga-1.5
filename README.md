# minix-amiga-1.5

Amiga MINIX 1.5.10 – the original 1991 Prentice Hall release for the
Commodore Amiga – ready to run in the FS-UAE emulator, without a Kickstart
ROM: FS-UAE's built-in AROS replacement ROM is enough to boot it.

This repository contains:

- `emu/images/1.5.10/` – the original floppy images (boot disk archive
  `amboot.lha` and disks 02–09), unchanged, from
  http://download.minix3.org/previous-versions/Amiga/
- `utils/` – scripts that turn them into working copies for the emulator
  (a bootable ADF from `amboot.lha`, copies of the 720 KB floppies)
- `emu/config/` – the FS-UAE configuration
- `disk/` – the unpacked contents of the floppies, for reading
  the sources and files without an emulator


## License

MINIX was relicensed by Prentice Hall under a BSD license in April 2000
(announcement by Andrew S. Tanenbaum in comp.os.minix, 7 April 2000);
the license applies retroactively to all earlier versions, including
Amiga MINIX 1.5. See `LICENSE`.


## Quick start

```bash
sudo apt install fs-uae lhasa python3-venv git
git clone https://github.com/roodemol/minix-amiga-1.5
cd minix-amiga-1.5
utils/lha2adf.sh emu/images/1.5.10/amboot.lha emu/work/1.5.10/minix-boot.adf \
                 'BOOT:Minix -s 0 -e 512'
utils/copydisks.sh emu/images/1.5.10 emu/work/1.5.10
fs-uae emu/config/minix-1.5.10.fs-uae
```

FS-UAE boots from the working copies in `emu/work/1.5.10/`, created by the two
scripts: the boot disk `minix-boot.adf` in drive DF0: and the root disk
`disk02.img` in DF1:. The originals in `emu/images/1.5.10/` stay unchanged.

1. At “Insert ROOT diskette and hit RETURN (or specify bootdev)” type `2,9`
   and press RETURN. This tells MINIX to read the root disk from drive DF1:
   (device 2,9); it selects a drive, it does not swap disks.
2. At “Please insert /usr diskette in drive 0. Then hit RETURN.” press
   **F12**, choose drive DF0: and the image `disk03.img`, close the menu and
   press RETURN. Disks are only ever swapped this way, in FS-UAE's F12 menu.
3. Log in as `root` with the password `Geheim`.

Details are in section 5.

---

## 0. Overview: how does Amiga MINIX boot?

| Part | Format | Read by |
|---|---|---|
| `amboot.lha` → **boot disk** | 880 KB AmigaDOS (OFS), volume name `BOOT` | the ROM (boot block, `startup-sequence`) and the MINIX loader `BOOT:Minix`, which loads `BOOT:minix.img` |
| `disk02` … `disk09` | 720 KB **PC format** (9 sectors per track), MINIX file system | the MINIX kernel itself (`kernel/floppy.c` reads IBM MFM directly through the Paula hardware) |

This means:

- The ROM is **only needed for starting**: it runs the `startup-sequence`,
  which calls the loader; the loader loads the kernel, and the kernel takes
  over the hardware. The AROS ROM built into FS-UAE is sufficient for that.
- The floppies 02–09 are **not** ADF files. FS-UAE recognises 737,280-byte
  images with the extension `.img` as PC floppies.
- The root disk is `disk02`, the `/usr` disk `disk03`.
- The unpacked file tree in `disk/` is for reading and is **not bootable**.
  Booting uses the images in `emu/images/1.5.10/`.

## 1. Required packages

```bash
sudo apt update
sudo apt install fs-uae lhasa python3-venv git
```

`utils/lha2adf.sh` checks that `lha` (package `lhasa`) and `python3-venv` are
installed.

## 2. Repository layout

```
minix-amiga-1.5/
├── LICENSE                      # MINIX BSD licence (Prentice Hall, April 2000)
├── README.md
├── EMULATION.md                 # this guide
├── .gitignore                   # excludes emu/work/
├── .gitattributes               # marks the images as binary
├── disk/                        # unpacked file tree (for reading)
├── utils/
│   ├── lha2adf.sh               # bootable ADF from an LHA archive
│   └── copydisks.sh             # working copies of the 720 KB floppies
└── emu/
    ├── images/
    │   └── 1.5.10/              # original images, unchanged
    │       ├── amboot.lha       #   boot disk (packed)
    │       ├── disk02 … disk09  #   720 KB PC floppies
    │       ├── README
    │       └── index.html
    ├── config/
    │   └── minix-1.5.10.fs-uae  # FS-UAE configuration
    └── work/                    # working copies, not in the repository
        └── 1.5.10/
            ├── minix-boot.adf
            └── disk02.img … disk09.img
```

The originals in `emu/images/` are never changed. Everything FS-UAE uses and
writes to is in `emu/work/`, which is created by the two scripts below.

## 3. Creating the working copies

All commands are run from the top directory of the repository.

### 3a. The boot disk

```bash
utils/lha2adf.sh emu/images/1.5.10/amboot.lha emu/work/1.5.10/minix-boot.adf \
                 'BOOT:Minix -s 0 -e 512'
```

The script unpacks the archive, replaces `s/startup-sequence` with the line
given as third argument, and writes everything to a new ADF with the OFS file
system, the volume name `BOOT` and the standard AmigaDOS boot block. It ends
with `used: 666` of 1760 blocks.

**Parameters:**

- **`emu/work/1.5.10/minix-boot.adf`** – working copies go to `emu/work/`,
  which is not part of the repository; the configuration expects the boot
  disk there.
- **`BOOT`** (fixed in the script) – the loader is called as `BOOT:Minix` and
  finds `BOOT:minix.img` through the volume name.
- **The third argument replaces the start-up sequence.** In the archive,
  `s/startup-sequence` only runs `BOOT:c/nag`, which displays “you will have
  to install it”. The original installation script `scripts/minstall1.3`
  would copy Workbench commands (`Copy`, `Assign`, `SetMap`, `Ram-Handler` …)
  to the disk – these are copyrighted and not needed here – and would install
  `s/REAL-startup-sequence`:

  ```
  BOOT:c/assign devs: BOOT:devs
  BOOT:c/setmap m_usa1
  BOOT:Minix -s 0 -e $200
  ```

  Only the last line is needed: it calls the MINIX loader. The first two lines
  would need Workbench commands and only set the keyboard layout.
- **`-s 0`** – *kernel destination address* (default `$000200`): the loader
  copies `minix.img` to address 0. The image starts with a complete 68000
  vector table and is linked for address 0, so the vector table ends up
  where the CPU expects it and MINIX takes over the machine.
- **`-e 512`** – *kernel execute address*: after copying, the loader jumps to
  0x200. The image holds zero words from 0x200 to 0x3FF, which the 68000
  executes as harmless `ori.b #0,d0`, up to the start-up code at 0x400, the
  address the reset vector points to.
- **`512` instead of `$200`** – the loader reads numbers with `$` as
  hexadecimal and all others as decimal (`strval()` in `src/minix/loader.c`),
  so both mean the same. The AROS shell, however, takes `$200` for an
  environment variable and asks for the volume `ENV:`; the Workbench 1.3 shell
  did not have such variables.

### 3b. The 720 KB floppies

```bash
utils/copydisks.sh emu/images/1.5.10 emu/work/1.5.10
```

The script copies every file of exactly 737,280 bytes (80 tracks × 2 sides ×
9 sectors × 512 bytes) from the original directory to the working directory
and adds the extension `.img`: `disk02` → `disk02.img` … `disk09` →
`disk09.img`. Other files such as `amboot.lha` or `README` are skipped.

**Parameters:**

- **Source `emu/images/1.5.10`** – the originals stay unchanged.
- **Target `emu/work/1.5.10`** – FS-UAE writes into these copies
  (`writable_floppy_images = 1`), so everything you change in MINIX ends up
  there.
- **Existing copies are kept** (message `kept: …`) so that your changes are
  not lost by running the script again. To start over with the originals,
  use `-f`:

  ```bash
  utils/copydisks.sh -f emu/images/1.5.10 emu/work/1.5.10
  ```

## 4. FS-UAE configuration

The configuration is in `emu/config/minix-1.5.10.fs-uae`. The essential
settings:

| Setting | Value | Why |
|---|---|---|
| `amiga_model` | `A500` | Amiga MINIX 1.5.10 is made for the 68000 |
| `chip_memory`, `slow_memory` | `512` each | 512 KB chip RAM + 512 KB slow RAM, like an A500 with memory expansion |
| `floppy_drive_count` | `2` | boot disk and root disk at the same time |
| `floppy_drive_0` | `minix-boot.adf` | DF0: – the Amiga boots from it |
| `floppy_drive_1` | `disk02.img` | DF1: – the root disk (`2,9` at the prompt) |
| `floppy_image_0` … `_8` | all nine images | list for swapping disks in the F12 menu |
| `writable_floppy_images` | `1` | write changes directly into the working copies instead of overlay files |
| `kickstart_file` | *not set* | FS-UAE then uses its built-in AROS ROM |

All paths are relative to the configuration file (FS-UAE resolves them
against its directory, not against the current directory), so it works
wherever the repository is cloned.

## 5. Starting and using MINIX

```bash
fs-uae emu/config/minix-1.5.10.fs-uae
```

1. AROS boots from DF0:, the boot disk `emu/work/1.5.10/minix-boot.adf` that
   `utils/lha2adf.sh` created from `amboot.lha` (section 3a). It runs the
   start-up sequence, which calls the MINIX loader, and the loader loads the
   kernel. After a little less than a minute (emulated floppy speed):

   ```
   Booting MINIX 1.5.  Copyright 1991 Prentice-Hall, Inc.
   Insert ROOT diskette and hit RETURN (or specify bootdev)
   ```

2. **Type `2,9` and press RETURN.** This is device major 2 (floppy), minor 9
   (`dd1`, drive DF1:), where `disk02` is. RETURN alone would read DF0: (`dd0`,
   2,8), which still holds the boot disk.

   MINIX loads the root disk into the RAM disk:

   ```
   Memory size = 1479K  MINIX = 153K  RAM disk = 160K  Available = 1166K
   RAM disk loaded.    Please remove root diskette.
   Please insert /usr diskette in drive 0.  Then hit RETURN.
   ```

3. **Insert the `/usr` disk:** press **F12** → drive DF0: → choose
   `disk03.img`, close the menu, then press RETURN. *(Not yet tested.)*
   If you press RETURN without swapping, `/etc/rc` reports “/dev/dd0 is not a
   valid file system” and errors about `/usr/bin/…`; the system still runs,
   just without `/usr`.

4. Log in:

   | User | Password |
   |---|---|
   | `root` | `Geheim` |
   | `ast` | `Wachtwoord` |

5. To quit: `sync`, then close FS-UAE. The changes are in the working copies
   in `emu/work/1.5.10/`; the originals remain unchanged.

The keyboard has the US layout (the start-up sequence does not run
`SetMap`).

### What is on which floppy

From `emu/images/1.5.10/README`, checked against the root directories of the
images:

| Image | Contents | Main directories |
|---|---|---|
| `amboot.lha` | the contents of the boot disk as an LHA archive: loader `Minix`, boot image `minix.img`, keymaps, transfer tool, loader sources. `utils/lha2adf.sh` turned it into the boot disk `emu/work/1.5.10/minix-boot.adf` (section 3a), with the start-up sequence that calls the loader | – |
| `disk02` | root file system | `/bin` (cat, cp, login, ls, sh …), `/dev`, `/etc` (rc, passwd, mount …), empty `/usr` and `/user` as mount points |
| `disk03` | system binaries #1 – the `/usr` disk | `bin/` (66 commands), `adm/`, `ast/` (home of user ast) |
| `disk04` | system binaries #2 | `bin/` (44 commands: cal, cdiff, backup …) |
| `disk05` | system binaries #3 | `bin/` (50 commands: mdb, nroff, patch …), `lib/` (fortune data, more help, tmac) |
| `disk06` | C compiler | `bin/` (`cc`, `as`), `lib/` (`cpp`, `cem`, `cg`, `ld` …), `include/` |
| `disk07` | operating system sources | `src/` (kernel, fs, mm, lib, test, tools, commands) |
| `disk08` | command sources #1 | `src/commands/` |
| `disk09` | command sources #2 | `src/commands/`, `lib/dictionary.Z` |

`/etc/rc` mounts only the `/usr` disk (`disk03`) in drive 0. The other disks
can be mounted by hand, for example in DF1: (free after the root disk has
been loaded into the RAM disk) on the empty directory `/user`:

```
/etc/mount /dev/dd1 /user
ls /user/bin
/etc/umount /dev/dd1
```

