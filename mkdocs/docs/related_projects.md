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

* [snapraid](https://www.snapraid.it/)
* [rclone](https://rclone.org/)
  * rclone's [union](https://rclone.org/union/) feature is based on
    mergerfs policies
* [ZFS](https://openzfs.org/): Common to use ZFS w/ mergerfs
* [UnRAID](https://unraid.net): While UnRAID has its own union
  filesystem it isn't uncommon to see UnRAID users leverage mergerfs
  given the differences in the technologies.
* For a time there were a number of Chia miners recommending mergerfs
* [cloudboxes.io](https://cloudboxes.io/wiki/how-to/apps/set-up-mergerfs-using-ssh)


## Distributions including mergerfs

mergerfs can be found in the
[repositories](https://pkgs.org/download/mergerfs) of [many
Linux](https://repology.org/project/mergerfs/versions) (and maybe
FreeBSD) distributions.

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
* [NixOS](https://search.nixos.org/packages?channel=22.11&show=mergerfs&from=0&size=50&sort=relevance&type=packages&query=mergerfs)
* [Guix]()
* [Slackware](https://slackbuilds.org/repository/15.0/system/mergerfs/?search=mergerfs)
