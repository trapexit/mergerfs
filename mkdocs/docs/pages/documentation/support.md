# SUPPORT

Filesystems are complex and difficult to debug. mergerfs, while being
just a proxy of sorts, can be difficult to debug given the large
number of possible settings it can have itself and the number of
environments it can run in. When reporting on a suspected issue
**please** include as much of the below information as possible
otherwise it will be difficult or impossible to diagnose. Also please
read the above documentation as it provides details on many previously
encountered questions/issues.

**Please make sure you are using the [latest
release](https://github.com/trapexit/mergerfs/releases) or have tried
it in comparison. Old versions, which are often included in distros
like Debian and Ubuntu, are not ever going to be updated and the issue
you are encountering may have been addressed already.**

**For commercial support or feature requests please [contact me
directly.](mailto:support@spawn.link)**

#### Information to include in bug reports

- [Information about the broader problem along with any attempted
  solutions.](https://xyproblem.info)
- Solution already ruled out and why.
- Version of mergerfs: `mergerfs --version`
- mergerfs settings / arguments: from fstab, systemd unit, command
  line, OMV plugin, etc.
- Version of the OS: `uname -a` and `lsb_release -a`
- List of branches, their filesystem types, sizes (before and after issue): `df -h`
- **All** information about the relevant paths and files: permissions, ownership, etc.
- **All** information about the client app making the requests: version, uid/gid
- Runtime environment:
  - Is mergerfs running within a container?
  - Are the client apps using mergerfs running in a container?
- A `strace` of the app having problems:
  - `strace -fvTtt -s 256 -o /tmp/app.strace.txt <cmd>`
- A `strace` of mergerfs while the program is trying to do whatever it is failing to do:
  - `strace -fvTtt -s 256 -p <mergerfsPID> -o /tmp/mergerfs.strace.txt`
- **Precise** directions on replicating the issue. Do not leave **anything** out.
- Try to recreate the problem in the simplest way using standard programs: `ln`, `mv`, `cp`, `ls`, `dd`, etc.

#### Contact / Issue submission

- github.com: https://github.com/trapexit/mergerfs/issues
- discord: https://discord.gg/MpAr69V
- reddit: https://www.reddit.com/r/mergerfs

#### Donations

https://github.com/trapexit/support

Development and support of a project like mergerfs requires a
significant amount of time and effort. The software is released under
the very liberal ISC license and is therefore free to use for personal
or commercial uses.

If you are a personal user and find mergerfs and its support valuable
and would like to support the project financially it would be very
much appreciated.

If you are using mergerfs commercially please consider sponsoring the
project to ensure it continues to be maintained and receive
updates. If custom features are needed feel free to [contact me
directly](mailto:support@spawn.link).
