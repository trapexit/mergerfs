# Configuration and Policies

## What policies should I use?

Unless you're doing something more niche the average user is probably
best off using `mfs` for `category.create`. It will spread files out
across your branches based on available space. Use `mspmfs` if you
want to try to colocate the data a bit more. You may want to use `lus`
if you prefer a slightly different distribution of data if you have a
mix of smaller and larger filesystems. Generally though `mfs`, `lus`,
or even `rand` are good for the general use case. If you are starting
with an imbalanced pool you can use the tool **mergerfs.balance** to
redistribute files across the pool. That said "balancing" really
should not be needed. There is really no inherent issue with being
unbalanced. "Optimal" layout is related to usage patterns and most
people don't have patterns that are performance sensitive or specific
enough to do anything about. Additionally, the balance tool is very
simplistic and was not written to be as serious tool that accounts for
edge cases.

If you really wish to try to colocate files based on directory you can
set `func.create` to `epmfs` or similar and `func.mkdir` to `rand` or
`eprand` depending on if you just want to colocate generally or on
specific branches. Either way the _need_ to colocate is rare. For
instance: if you wish to remove the device regularly and want the data
to predictably be on that device or if you don't use backup at all and
don't wish to replace that data piecemeal. In which case using path
preservation can help but will require some manual
attention. Colocating after the fact can be accomplished using the
**mergerfs.consolidate** tool. If you don't need strict colocation
which the `ep` policies provide then you can use the `msp` based
policies which will walk back the path till finding a branch that
works.

Ultimately there is no correct answer. It is a preference or based on
some particular need. mergerfs is very easy to test and experiment
with. I suggest creating a test setup and experimenting to get a sense
of what you want.

`epmfs` is the default `category.create` policy because `ep` policies
are not going to change the general layout of the branches. It won't
place files/dirs on branches that don't already have the relative
branch. So it keeps the system in a known state. It's much easier to
stop using `epmfs` or redistribute files around the filesystem than it
is to consolidate them back.


## What settings should I use?

Depends on what features you want. Generally speaking, there are no
"wrong" settings. All settings are feature related. The best bet is to
read over the available options and choose what fits your
situation. If something isn't clear from the documentation please
reach out and the documentation will be improved.

For the average person the settings described in the [Quick
Start](../quickstart.md) are sufficient.


## Why are all my files ending up on 1 filesystem?!

Did you start with empty filesystems? Did you explicitly configure a
`category.create` policy? Are you using an `existing path` / `path
preserving` policy?

The default create policy is `epmfs`. That is a path preserving
algorithm. With such a policy for `mkdir` and `create` with a set of
empty filesystems it will select only 1 filesystem when the first
directory is created. Anything, files or directories, created in that
first directory will be placed on the same branch because it is
preserving paths.

This may catch new users off guard but this policy is the safest
policy to start with as it will not change the general layout of the
underlying filesystems. If you do not care about path preservation
(most shouldn't) and wish your files to be spread across all your
filesystems change to `mfs` or similar
[policy](../config/functions_categories_and_policies.md). If you do
want path preservation you'll need to perform the manual act of
creating paths on the filesystems you want the data to land on before
transferring your data.
