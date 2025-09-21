---
name: Bug report
about: For the reporting of behavior not as described
title: ''
labels: bug, investigating
assignees: ''

---

**Describe the bug**

A clear and concise description of the unexpected behavior.

**Please be sure to use latest release of mergerfs to ensure the issue still exists. Not your distro's latest but the latest official release.**

The master branch is **not** to be considered production ready. Feel free to file bug reports but do so indicating clearly that you are testing unreleased code.


**To Reproduce**

Steps to reproduce the behavior. List **all** steps to reproduce. **All** settings.

Please simplify the reproduction as much as possible.
 - Unless it is dependenat on multiple branches, use a single branch
 - Reproduce with standard tooling if possible (touch,truncate,rm,rmdir,ln,etc.) Having to install 3rd party software will make debugging more difficult.


**Expected behavior**

A clear and concise description of the expected behavior.


**System information:**

 - OS, kernel version: `uname -a`
 - mergerfs version: `mergerfs -V`
 - mergerfs settings
 - List of drives, filesystems, & sizes:
   - `df -h`
   - `lsblk -f`
 - A strace of the application having a problem:
   - `strace -fvTtt -s 256 -o /tmp/app.strace.txt <cmd>`
   - `strace -fvTtt -s 256 -o /tmp/app.strace.txt -p <appPID>`
 - strace of mergerfs while app tried to do it's thing:
   - `strace -fvTtt -s 256 -p <mergerfsPID> -o /tmp/mergerfs.strace.txt`

**Additional context**

Add any other context about the problem here.
