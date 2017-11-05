General description
===================



Expected behavior
=================



Actual behavior
===============



**Precise** steps to reproduce the behavior
===========================================

> Explicitly list **all** steps to reproduce. Preferably create a minimal example of the problem using standard command line tools. The more variables (apps, settings, etc.) that are involved the more difficult it is to debug. Also, **please** be sure to have read all of the README. It contains a lot of information regarding known system and user issues.



System information
==================
Please provide as much of the following information as possible:

* [ ] mergerfs version: `mergerfs -V`
* [ ] mergerfs settings: `cat /etc/fstab` or the command line arguments
* [ ] Linux version: `uname -a`
* [ ] Versions of any additional software being used
* [ ] List of drives, filesystems, & sizes: `df -h`
* [ ] strace of application having problem: `strace -f -o /tmp/app.strace.txt <cmd>` or `strace -f -p <appPID> -o /tmp/app.strace.txt`
* [ ] strace of mergerfs while app tried to do it's thing: `strace -f -p <mergerfsPID> -o /tmp/mergerfs.strace.txt`

