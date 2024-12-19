# trapexit's (mergerfs' author)

## Current setup

- SilverStone Technology CS380B-X V2.0 case
- Intel Core i7-4790S
- 32GB DDR3 RAM
- LSI SAS 9201-16e
  - 8 SATA connections to the CS380B backplane
  - 8 SATA connections to a generic 8-bay enclosure (similar to a Sans Digital 8-bay enclosure)
  - Connections via SAS to SATA breakout cables fished through the back. Not elegant but cost effective. SAS SCSI cutout boards are difficult to find and would add $50 to $100 to the cost.
- Marvell 88SE9230 PCIe SATA 6Gb/s Controller on motherboard
  - 4 SATA connections to a [StarTech SATSASBP425](https://www.amazon.com/StarTech-com-4-Bay-Mobile-Backplane-Drives/dp/B00X7B3CUE)
- ASMedia ASM1062 SATA Controller on motherboard
  - 4 SATA connections to a second [StarTech SATSASBP425](https://www.amazon.com/StarTech-com-4-Bay-Mobile-Backplane-Drives/dp/B00X7B3CUE)
  - 1 MSATA connection on the motherboard
- NVidia Quadro P2000 (for hardware transcoding in Plex, Jellyfin, etc.)
- Mix of 3.5" SATA HDD: 8TB - 14TB
- Mix of 2.5" SATA HDD: 2TB - 5TB
- Mix of 2.5" SATA SSD:
  - primary boot drive, backup boot drive, application specific caches
  - Some of the SSDs are used enterprise drives which can often be found for a reasonable price on eBay
- Mix of 2.5" U.2 NVME: 3x 2TB Intel P4510, 1x 3.84TB Dell P5500
  - Connected via a [Ceacent ANU28PE16 NVMe SSD Riser SFF8643 to SFF8639](https://www.aliexpress.us/item/2255800570129198.html)
  - Have a [IcyDock MB931U-1VB](https://www.icydock.com/goods.php?id=363) for using U.2 NVME drives externally
- All drives formatted with EXT4 to make recovery easier in case of failure
  - `mkfs.ext4 -L DRIVE_SERIAL_NUMBER -m 0 /dev/DEV`
- HDDs, some SSDs, some NVMEs merged together in a single mergerfs mount
  - branches-mount-timeout=300
  - cache.attr=120
  - cache.entry=120
  - cache.files=per-process
  - cache.readdir=true
  - cache.statfs=10
  - category.create=pfrd
  - dropcacheonclose=true
  - fsname=media
  - lazy-umount-mountpoint=true
  - link_cow=true
  - readahead=2048
- some SSDs/NVMes used for bespoke purposes such as main storage for Docker/container config storage and caching (Plex transcoding, etc.)
- Filesystem labels are set to the serial number of the drive for easy identification
- Drives mounted to:
  - /mnt/hdd/SIZE-LABEL
  - /mnt/sdd/SIZE-LABEL
  - /mnt/nvme/SIZE-LABEL
  - ex: /mnt/hdd/8TB-ABCDEF
- Total drives in main mergerfs pool: 24
- Total storage combined in main mergerfs pool: 155TB
- RAM usage by mergerfs under load: 512MB - 1GB of resident memory

## Old setup

- Core i7 3770s
- 16GB RAM
- 4 Port ASMedia Technology 106x eSATA PCIE 4x card
- 4x [ICYCube MB561U3S-4SB R1 Quad Bay enclosure](https://www.icydock.com/goods.php?id=219)

NOTES: The eSATA enclosure setup was easier to manage physically as the enclosures are smaller but the LSI SAS HBA & generic enclosure setup is more reliable/stable, more performant, and actually cost less. Port multipliers tend to behave poorly with different brand controllers (if they work at all). They can perform poorly if a drive is bad leading to the other drives acting as if they have issues leading to a full hard reset of the computer and enclosure to 'fix'. Port multiplier enclosures, over USB, tend not to support hot swapping and the drives will all be reset if a drive is swapped.

---

# (´･ω･`)

- 2x Intel(R) Xeon(R) CPU X5690 @ 3.47GHz
- 64G RAM
- Chassis: 847E16-R1K28LPB
  - 36 bays + 2 sytem bays
  - SAS2008
  - X8DTH
- Drives
  - 26x 8TB Data
    - luks
    - btrfs: `mount -ospace_cache=v2,noatime,rw`
  - 6x 8TB Parity
    - luks
    - ext4: `mkfs.ext4 -J size=4 -m 0 -i 67108864 -L <LABEL> <DEVICE>`
- Software
  - `mergerfs` all data disks to a single root: `-o defaults,allow_other,category.create=msplfs,minfreespace=100G,use_ino,dropcacheonclose=true,ignorepponrename=true,moveonenospc=mspmfs`
  - `snapraid` all drives and 6 parity
  - `snapraid-btrfs` btrfs snapshots+snapraid
- Misc
  - 1.2GB/s max with 2x SAS2 connections
  - ~56-65 hours for full sync
    - 1-4 hours on "small" changes
  - a fuckton of reads against the hdds if you change too much

Note: Buy good chassis with even better backplanes. The backplane 847E16-R1K28LPB uses, can cascade 24 drives and allow access over a single SAS2 connection.
You can interconnect the front- and backplane and it will cascade 36 drives over a single SAS2 connection. But most HBAs have 2 connections, so you can do 1.2GB/s.
The backplanes actually have SAS3, but the cards are.... well... expensive. But if you got one, ayyy 2x 12Gbit connection to the backplane. Or maybe 4x 12Gbit if you are up to it.

Tweak your snapraid autosave to ~4-6TB, otherwise it will take a long ass time.

Oh, yeah. If you got all the drives and the right connection to your backpanel get ready for cpu carnage.

![rape](https://i.imgur.com/ClJAbmF.png "the rape")
