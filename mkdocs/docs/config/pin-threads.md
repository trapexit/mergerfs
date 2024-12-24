# pin-threads

Simple strategies for pinning read and/or process threads. If process
threads are not enabled then the strategy simply works on the read
threads. Invalid values are ignored.

* `pin-threads=R1L`: All read threads pinned to a single logical CPU.
* `pin-threads=R1P`: All read threads pinned to a single physical CPU.
* `pin-threads=RP1L`: All read and process threads pinned to a single logical CPU.
* `pin-threads=RP1P`: All read and process threads pinned to a single physical CPU.
* `pin-threads=R1LP1L`: All read threads pinned to a single logical
  CPU, all process threads pinned to a (if possible) different logical
  CPU.
* `pin-threads=R1PP1P`: All read threads pinned to a single physical
  CPU, all process threads pinned to a (if possible) different logical
  CPU.
* `pin-threads=RPSL`: All read and process threads are spread across
  all logical CPUs.
* `pin-threads=RPSP`: All read and process threads are spread across
  all physical CPUs.
* `pin-threads=R1PPSP`: All read threads are pinned to a single
  physical CPU while process threads are spread across all other
  physical CPUs.
