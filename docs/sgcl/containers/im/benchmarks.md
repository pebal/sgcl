# Benchmarks: the immutable containers

The setup, the machine, the environments and how the timers are read are described with [the benchmarks of the engine](../../../garbage_collector/benchmarks.md); the numbers here were taken the same way on 19 September 2026 with `benchmarks/compare.sh` (`CASES="im"`, the best of three runs of every cell, the machine under its desktop load), from `bench_im`, whose source is `benchmarks/im/im.cpp`: `bench_im vector|map sgcl|immer|immer-unsafe|std [n]`, `bench_im list sgcl|std [n]`. What is measured is the cost of a version: every `push_back`, `set` and `insert` returns a new container and lets the old one go, so the number is what a change costs in a program whose state is a value.

The map against `std::map` (`bench_im map std`), one thread, 200,000 random `long` keys: an insertion a version each (the old one let go of), a lookup of a key present, the map built from a range at once, and `std::map` doing the same in place, ns per operation:

| | `im::map` | `std::map` |
|---|---|---|
| insert | 480 | 153 |
| find | 24 | 91 |
| built at once, per element | 91 | 149 |

The map pays for its versions where its page says: an insertion copies the path, four nodes, the links taken without the barrier and each source node shaded once, 480 ns against the red-black tree's 153 (702 before the unshaded copy, a hundred pointers stored each with the write barrier); its lookup is the hash trie's walk of four nodes, 24 ns against the tree's 91 at that size; the constructor from a range was an insert per element at first, 698 ns each, and builds the trie at once now, the elements sorted by their hashes in the trie's order and every node made once at its size: 90 ns per element, of which the sort is the larger part.

The immutable containers against [immer](https://github.com/arximboldi/immer), the C++ library of the same structures (Clojure's vector, a hash array mapped trie) over reference counts: `bench_im` (`CASES="im"`; the immer variants are built with `-DSGCL_IMMER_INCLUDE=<checkout>`), one thread, the best of three runs, ns per element. `immer` is its default memory policy, atomic counts and thread-safe versions as `im`'s are; `immer-unsafe` counts without atomics. The vector: a million elements pushed one version each, read at random positions, `set` at random positions one version each, and a million built at once (`im`'s range constructor, immer's transient); the map: 200,000 random `long` keys inserted one version each, found, and built at once:

| | `im::vector` | `immer::vector` | `immer-unsafe` |
|---|---|---|---|
| push_back | 22 | 31 | 24 |
| get | 5.2 | 3.6 | 4.1 |
| set | 133 | 308 | 227 |
| built at once | 4.9 | 2.8 | 3.2 |

| | `im::map` | `immer::map` | `immer-unsafe` |
|---|---|---|---|
| insert | 480 | 452 | 383 |
| find | 24 | 16 | 15 |
| built at once | 91 | 154 | 146 |

The list against `std::forward_list` in place (immer has no list): a million `long`s pushed in front one version each, the million walked, and popped from the front one version each, ns per element:

| | `im::list` | `std::forward_list` |
|---|---|---|
| push_front | 13–16 | 11 |
| walk, per cell | 1.5 | 0.4 |
| pop_front | 7 | 8 |

A `push_front` is one managed allocation and one barriered store against a `malloc` and a plain store; a `pop_front` frees nothing, where the mutable list frees a node; the walk is the same chain of dependent loads on both, the cells in the order they were made — the gap there is under investigation with the vector's reads (the collector's threads at work on the versions dropped, and a load through a `tracked_ptr`).

What the numbers say. A change copies the nodes on a path, 32 words each; `im` copies them without the write barrier and shades the source node once ([vector](vector.md), [tracked_ptr: shade](../../core/tracked_ptr.md#shade-storep-barrieroff)), where immer copies the words and bumps a reference count per child: `set` 133 against 227–308, `push_back` 22 against 24–31, the map's insert level with immer's thread-safe policy. Building at once: `im::map` sorts by hash and builds the trie bottom up, 91 against immer's transient's 150; `im::vector`'s range constructor fills leaves in order, 4.9 against 2.8–3.2 (a managed allocation per leaf and branch against immer's free list). The reads: `im` is behind, 5.2 against 3.6 on the vector and 24 against 16 on the map — a load through a `tracked_ptr` at every level and, in the map, an entry that holds its link and its element apart; the layout of the HAMT node is the open item.

