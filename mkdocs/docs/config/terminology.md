# Terminology

- `branch`: A base path used in the pool. Keep in mind that mergerfs
  does not work on devices or even filesystems but on paths. It can
  accomidate for multiple paths pointing to the same filesystem.
- `pool`: The mergerfs mount. The union of the branches. The instance
  of mergerfs. You can have as many pools as you wish.
- `relative path`: The path in the pool relative to the branch and mount.
- `function`: A filesystem call (open, unlink, create, getattr, rmdir, etc.)
- `category`: A collection of functions based on basic behavior (action, create, search).
- `policy`: The algorithm used to select a file or files when performing a function.
- `path preservation`: Aspect of some policies which includes checking the path for which a file would be created.
