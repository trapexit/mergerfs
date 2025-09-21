# link-exdev

If using path preservation and a `link` fails with `EXDEV` make a call
to `symlink` where the target is the `oldlink` and the `linkpath` is
the newpath. The target value is determined by the value of
`link-exdev`.

* `link-exdev=passthrough`: Return EXDEV as normal.
* `link-exdev=rel-symlink`: A relative path from the newpath.
* `link-exdev=abs-base-symlink`: An absolute value using the
  underlying branch.
* `link-exdev=abs-pool-symlink`: An absolute value using the mergerfs
  mount point.
* Defaults to `passthrough`.
  
**NOTE:** It is possible that some applications check the file they
link. In those cases, it is possible it will error or complain.
