---
name: Bug report
about: For the reporting of behavior not as described
title: ''
labels: bug, investigating
assignees: ''

---

**Describe the bug**

A clear and concise description of what the bug is. Make sure you've upgraded to the latest release to ensure the issue still exists.

The master branch is **not** to be considered production ready. Feel free to file bug reports but do so indicating clearly that you are testing unreleased code.


**To Reproduce**

Steps to reproduce the behavior. List **all** steps to reproduce. **All** settings.

Please simplify the reproduction as much as possible. 
 - Unless it is dependenat on multiple branches, use a single branch
 - Use basic tooling if possible (touch,truncate,rm,rmdir,ln,etc.)


**Expected behavior**

A clear and concise description of what you expected to happen.


**System information:**

 - OS, kernel version: `uname -a`
 - mergerfs version: `mergerfs -V`
 - mergerfs settings
 - List of drives, filesystems, & sizes:
   - `df -h`
   - `lsblk`
 - A strace of the application having a problem: 
   - `strace -fvTtt -s 256 -o /tmp/app.strace.txt <cmd>`
   - `strace -fvTtt -s 256 -o /tmp/app.strace.txt -p <appPID>`
 - strace of mergerfs while app tried to do it's thing:
   - `strace -fvTtt -s 256 -p <mergerfsPID> -o /tmp/mergerfs.strace.txt`

**Additional context**

Add any other context about the problem here.
