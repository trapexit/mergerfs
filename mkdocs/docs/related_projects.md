# Related Projects

## Projects using mergerfs

* [Lakka.tv](https://lakka.tv/): A turnkey software emulation Linux
  distribution. Used to pool user and local storage. Also includes my
  other project [Opera](https://retroarch.com/). A 3DO emulator.
* [OpenMediaVault](https://www.openmediavault.org): A network attached
  storage (NAS) solution based on Debian Linux. They provide plugins
  to manage mergerfs.
* [CasaOS](https://casaos.io): "A simple, easy to use, elegant open
  source home cloud system." Has added initial integration with
  mergerfs to create pools from existing filesystems.
* [ZimaOS](https://github.com/IceWhaleTech/zimaos-rauc): A more
  commercially focused NAS OS by the authors of CasaOS at [Ice
  Whale](https://www.zimaboard.com/).
* [Cosmos Cloud](https://cosmos-cloud.io/): Cosmos "take the chore out
  of selfhosting, with automated maintenance and fully secured setup
  out of the box. It even integrates to your existing setup."


## Software and services commonly used with mergerfs

* [snapraid](https://www.snapraid.it/): a backup program designed for
  disk arrays, storing parity information for data recovery in the
  event of up to six disk failures.
* [nonraid](https://github.com/qvr/nonraid): NonRAID is a fork of the
  unRAID system's open-source md_unraid kernel driver enabling
  UnRAID-style storage arrays with parity protection outside of the
  commercial UnRAID system.
* [rclone](https://rclone.org/): a command-line program to manage
  files on cloud storage. It is a feature-rich alternative to cloud
  vendors' web storage interfaces. rclone's
  [union](https://rclone.org/union/) feature is based on mergerfs
  policies.
* [ZFS](https://openzfs.org/): A popular filesystem and volume
  management platform originally part of Sun Solaris and later ported
  to other operating systems. It is common to use ZFS with
  mergerfs. ZFS for important data and mergerfs pool for replacable
  media.
* [Proxmox](https://www.proxmox.com/): Proxmox is a popular, Debian
  based, virtualization platform. Users tend to install mergerfs on
  the host and pass the mount into containers.
* [UnRAID](https://unraid.net): "Unraid is a powerful, easy-to-use
  operating system for self-hosted servers and network-attached
  storage." While UnRAID has its own union filesystem it isn't
  uncommon to see UnRAID users leverage mergerfs given the differences
  in the technologies. There is a [plugin available by
  Rysz](https://forums.unraid.net/topic/144999-plugin-mergerfs-for-unraid-support-topic/)
  to ease installation and setup.
* [TrueNAS SCALE](https://www.truenas.com/truenas-scale/): An
  enterprise focused NAS operating system with OpenZFS support. A Some
  users are requesting mergerfs be [made part
  of](https://forums.truenas.com/t/add-unionfs-or-mergerfs-and-rdam-enhancement-then-beat-all-other-nas-systems/23218)
  TrueNAS.
* For a time there were a number of Chia miners recommending mergerfs.
* [cloudboxes.io](https://cloudboxes.io): VPS provider. Includes
  details [on their
  wiki](https://cloudboxes.io/wiki/how-to/apps/set-up-mergerfs-using-ssh):
  on how to setup mergerfs.
* [QNAP](https://www.myqnap.org/product/mergerfs-apache83/): A company
  known for their turnkey, consumer focused NAS devices. Someone has
  created builds of mergerfs for different QNAP devices.


## Distributions including mergerfs

mergerfs can be found in the
[repositories](https://pkgs.org/download/mergerfs) of [many
Linux](https://repology.org/project/mergerfs/versions) distributions
and FreeBSD.

Note: Any non-rolling release based distro is likely to have
out-of-date versions.

* [Debian](https://packages.debian.org/bullseye/mergerfs)
* [Ubuntu](https://launchpad.net/ubuntu/+source/mergerfs)
* [Fedora](https://rpmsphere.github.io/)
* [T2](https://t2sde.org/packages/mergerfs)
* [Alpine](https://pkgs.alpinelinux.org/packages?name=mergerfs&branch=edge&repo=&arch=&maintainer=)
* [Gentoo](https://packages.gentoo.org/packages/sys-fs/mergerfs)
* [Arch (AUR)](https://aur.archlinux.org/packages/mergerfs)
* [Void](https://voidlinux.org/packages/?arch=x86_64&q=mergerfs)
* [NixOS](https://search.nixos.org/packages?type=packages&query=mergerfs)
* [Guix]()
* [Slackware](https://slackbuilds.org/repository/15.0/system/mergerfs/?search=mergerfs)
* [FreeBSD](https://www.freshports.org/filesystems/mergerfs)
