# Support

Filesystems are complex, as are the interactions software have with
them, and therefore difficult to debug. When reporting on a suspected
issue **please** include as much of the below information as possible
otherwise it will be difficult or impossible to diagnose. Also please
read the documentation as it provides details on many previously
encountered questions/issues.

**Make sure you are using the [latest
release](https://github.com/trapexit/mergerfs/releases) or have tried
it in comparison. Old versions, which are often included in distros
like Debian and Ubuntu, are not ever going to be updated and the issue
you are encountering may have been addressed already.**

**For commercial support or feature requests please [contact me
directly.](mailto:support@spawn.link)**


### Information to include in bug reports

* [Information about the broader problem along with any attempted
  solutions.](https://xyproblem.info)
* Solution already ruled out and why.
* The details from the output of the
  [mergerfs.collect-info](tooling.md#mergerfscollect-info) tool.
* **All** information about the relevant paths and files: permissions,
  ownership, etc. Some of this is collected by the tool but only the
  branch root details.
* **All** information about the application making the requests:
  version, uid/gid. Try though to replicate any issues with common
  core utilities.
* Try to recreate the problem in the simplest way using standard
  programs: `ln`, `mv`, `cp`, `ls`, `dd`, etc. With the most simple
  mergerfs setup possible such as using only a single branch, well
  supported branch filesystem, maybe reduce to using a single thread,
  etc.
* Runtime environment:
    * Is mergerfs running within a container?
    * Are the client apps using mergerfs running in a container?
* A `strace` of the app having problems:
    * `strace -fvTtt -s 256 -o /tmp/app.strace.txt <cmd>`
* A `strace` of mergerfs **before** and **while** the program is
  trying to do whatever it is failing to do:
    * `strace -fvTtt -s 256 -p <mergerfsPID> -o
      /tmp/mergerfs.strace.txt`
* If mergerfs is blocking or appears stuck:
    * `gdb -q --batch -ex "set pagination off" -ex "thread apply all
      bt full" -ex quit --pid <mergerfsPID> > /tmp/mergerfs.stacktrace.txt`
* **Precise** step-by-step directions on replicating the issue. Do not
  leave **anything** out.


### Contact / Issue submission

In order of preference.

* github.com: [https://github.com/trapexit/mergerfs/issues](https://github.com/trapexit/mergerfs/issues)
* discord: [https://discord.gg/MpAr69V](https://discord.gg/MpAr69V)
* reddit: [https://www.reddit.com/r/mergerfs](https://www.reddit.com/r/mergerfs)
