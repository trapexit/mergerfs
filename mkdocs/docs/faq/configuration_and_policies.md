# Configuration and Policies

## What settings should I use?

Depends on what features you want. Generally, there are no "good",
"bad", "inefficient", or "optimal" settings. Options are almost
exclusively functional. Meaning they change the behavior of the
software. It is best to read over the available options and choose
what fits your use case. If something is not clear from the
documentation please reach out and the documentation will be improved.

The settings described in the [Quick Start](../quickstart.md) are
sufficient for most users.

Filesystems are complex and use cases numerous. There simply is no way
to provide a singular setup that works for all situations. Since
mergerfs [does not impact](usage_and_functionality.md) the underlying
filesystems and can be added or removed without any impact it is
extremely easy to test and experiment with different settings.


## Why is pfrd the default create policy?

Originally the default was `epmfs` however it was found to cause
significant confusion among new users who expected mergerfs to always
choose a branch with available space. For mergerfs v2.41.0 it was
decided to change the default to the less restrictive policy `pfrd`.


## How can I ensure files are collocated on the same branch?

Many people like the idea of ensuring related files, such as all the
files to a TV show season or songs in an album, are stored on the same
storage device. However, most people have no actual need for this
behavior.

1. If you backup your data it is extremely likely your backup solution
   can restore only those files you are missing.
2. Software such as **Sonarr** can manage the downloading and post
   processing of bespoke episodes which may be missing in a
   season. Either by downloading the episode individually if available
   or by downloading a full season.
3. There is no benefit to keeping files collocated with regard to
   drive spinup, caching, or other secondary concern.

The main use case for wanting collocation is where the branch is going
to be removed from the pool and you wish to have all data from some
logical set on that device. Such as you intend to take a drive out of
the pool to take on a trip and want a whole show on the
drive. However, even in these situations you typically end up needing
to curate the files anyway because it has show A but not show B.

All that said you can accomplish collocation to varying degrees using
the following methods:

1. Use
   [mergerfs.consolidate](https://github.com/trapexit/mergerfs-tools/blob/master/src/mergerfs.consolidate)
   when consolidation is needed.
2. Use a `msp` create policy.
3. Use `epmfs` or other `ep` create policy and manually create paths
   on the branches directly.
4. Use a `ep` `create` policy and `rand` for `mkdir`.


## How can I balance files across the pool?

Similar to collocation there is generally little reason to balance
files.

1. Since prediction of a filesystem's death or loss of data is near
   impossible there is little reason to balance in hopes of limiting
   data loss.
2. While performance could be impacted by having too much reading or
   writing happen to singular underlying filesystems balancing won't
   help unless you have the ability to manage the access patterns to
   the pool.
3. Over time most configurations will lead to a random distribution of
   files across the branches which is effectively "balancing."

If you wish to move files around or balance the pool you can:

1. Use `rand` or `pfrd` create policies and just use your system as
   normal.
2. Write simple scripts using rsync or similar to move files around as
   you wish.
3. Use
   [mergerfs.balance](https://github.com/trapexit/mergerfs-tools/blob/master/src/mergerfs.balance). Keep
   in mind that this tool is really just an example of how to
   accomplish such a task. The tool does not keep track of links so
   you may encounter an increase in used space if you rely on links to
   reduce redundancy. However, you can run a file dedup program like
   [rdfind](https://github.com/pauldreik/rdfind) to restore the links.
