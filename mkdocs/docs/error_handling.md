# Error Handling

POSIX filesystem functions offer a single return code meaning that
there is some complication regarding the handling of multiple branches
as mergerfs does. It tries to handle errors in a way that would
generally return meaningful values for that particular function.

### chmod, chown, removexattr, setxattr, truncate, utimens

1. if no error: return 0 (success)
2. if no successes: return first error
3. if one of the files acted on was the same as the related search function: return its value
4. return 0 (success)

While doing this increases the complexity and cost of error handling,
particularly step 3, this provides probably the most reasonable return
value.

### unlink, rmdir

1. if no errors: return 0 (success)
2. return first error

Older versions of mergerfs would return success if any success occurred
but for unlink and rmdir there are downstream assumptions that, while
not impossible to occur, can confuse some software.

### others

For search functions, there is always a single thing acted on and as
such whatever return value that comes from the single function call is
returned.

For create functions `mkdir`, `mknod`, and `symlink` which don't
return a file descriptor and therefore can have `all` or `epall`
policies it will return success if any of the calls succeed and an
error otherwise.
