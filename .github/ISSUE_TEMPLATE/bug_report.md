---
name: Bug report
about: For the reporting of behavior not as described
title: ''
labels: bug, investigating
assignees: ''

---

**Describe the bug**
A clear and concise description of what the bug is. Please do not file bugs for unreleased versions unless you're a tester or sure it's not a work in progress. The master branch is **not** to be considered in a production state.

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
 - List of drives, filesystems, & sizes: `df -h`
 - A strace of the application having a problem: `strace -f -o /tmp/app.strace.txt <cmd>` or `strace -f -p <appPID> -o /tmp/app.strace.txt`
 - strace of mergerfs while app tried to do it's thing: `strace -f -p <mergerfsPID> -o /tmp/mergerfs.strace.txt`

**Additional context**
Add any other context about the problem here.
