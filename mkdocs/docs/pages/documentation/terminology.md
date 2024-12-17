# TERMINOLOGY

- branch: A base path used in the pool.
- pool: The mergerfs mount. The union of the branches.
- relative path: The path in the pool relative to the branch and mount.
- function: A filesystem call (open, unlink, create, getattr, rmdir, etc.)
- category: A collection of functions based on basic behavior (action, create, search).
- policy: The algorithm used to select a file when performing a function.
- path preservation: Aspect of some policies which includes checking the path for which a file would be created.
