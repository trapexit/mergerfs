# minfreespace

default: `4G`

`minfreespace` is used to set a minimum free space threshold when
running create [policies](functions_categories_policies.md). Any
branch with free space less than the `minfreespace` value will be
skipped. This is the global default but individual branches can [have
their own value.](branches.md#minfreespace).

This is useful for a couple reasons.

* Since mergerfs does not split files across branches it is best to
  leave space to write a newly created file on a branch to minimize
  the risk of running out of space and needing
  [moveonenospc](moveonenospc.md).
* Some filesystems' performance degrades when filled. `minfreespace`
  provides a buffer so they are less likely to be filled.
* Similar to `ext4`'s reserved space option it minimizes the problems
  that come from running out of space.
