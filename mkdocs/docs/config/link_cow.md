# link_cow

* `link_cow=true|false`
* Defaults to `false`

This feature offers similar functionality to what
[cow-shell](https://manpages.ubuntu.com/manpages/noble/man1/cow-shell.1.html)
offers. 

When enabled if mergerfs is asked to open a file to write and the link
count on the file is greater than 1 it will copy the file to a
temporary new file and then rename it over the original. This will
atomically "break" the link. After that it will open the new file.
