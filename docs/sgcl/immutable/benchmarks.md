# Benchmarks: the immutable containers

The setup, the machine, the environments and how the timers are read are described with [the benchmarks of the engine](../../garbage_collector/benchmarks.md); the numbers here were taken the same way on 19 September 2026 with `benchmarks/compare.sh` (`CASES="im"`, the best of three runs of every cell, the machine under its desktop load), from `bench_immutable`, whose source is `benchmarks/immutable/immutable.cpp`: `bench_immutable vector|map sgcl|immer|immer-unsafe|std [n]`, `bench_immutable list sgcl|std [n]`. What is measured is the cost of a version: every `push_back`, `set` and `insert` returns a new container and lets the old one go, so the number is what a change costs in a program whose state is a value.

The map against `std::map` (`bench_immutable map std`), one thread, 200,000 random `long` keys: an insertion a version each (the old one let go of; `set`, which adds or replaces, since 2026-09-26, when `insert` came to keep an element that is there — the same work for a key that is absent), a lookup of a key present, the map built from a range at once, and `std::map` doing the same in place, ns per operation:

| | `immutable::map` | `std::map` |
|---|---|---|
| insert (`set`) | 480 | 153 |
| find | 24 | 91 |
| built at once, per element | 91 | 149 |

The map pays for its versions where its page says: an insertion copies the path, four nodes, the links taken without the barrier and each source node shaded once, 480 ns against the red-black tree's 153 (702 before the unshaded copy, a hundred pointers stored each with the write barrier); its lookup is the hash trie's walk of four nodes, 24 ns against the tree's 91 at that size; the constructor from a range was an insert per element at first, 698 ns each, and builds the trie at once now, the elements sorted by their hashes in the trie's order and every node made once at its size: 90 ns per element, of which the sort is the larger part.

The immutable containers against [immer](https://github.com/arximboldi/immer), the C++ library of the same structures (Clojure's vector, a hash array mapped trie) over reference counts: `bench_immutable` (`CASES="im"`; the immer variants are built with `-DSGCL_IMMER_INCLUDE=<checkout>`), one thread, the best of three runs, ns per element (the vector's rows, both libraries, taken again on 24 September with immer at `bd4fc74`, after the tails of `immutable::vector` took pages of their own). `immer` is its default memory policy, atomic counts and thread-safe versions as `im`'s are; `immer-unsafe` counts without atomics. The vector: a million elements pushed one version each, read at random positions, `set` at random positions one version each, and a million built at once (`im`'s range constructor, immer's transient); the map: 200,000 random `long` keys inserted one version each, found, and built at once:

| | `immutable::vector` | `immer::vector` | `immer-unsafe` |
|---|---|---|---|
| push_back | 22 | 31 | 25 |
| get | 3.2 | 3.4 | 4.0 |
| set | 131 | 429 | 259 |
| built at once | 4.9 | 2.9 | 3.5 |

| | `immutable::map` | `immer::map` | `immer-unsafe` |
|---|---|---|---|
| insert (`set`) | 480 | 452 | 383 |
| find | 24 | 16 | 15 |
| built at once | 91 | 154 | 146 |

The map's builder (`thaw`, `freeze`; [map](map.md#thaw-builder-freeze)) against immer's transient, `bench_immutable map builder|immer-builder` (`CASES="im"`, 24 September): the 200,000 keys built one at a time through it and frozen, a lookup of each in the map that comes out, and an edit of that map through one builder, a tenth of the keys given new values and a tenth erased, ns per element or per change; the last row is the same edit made a version a change:

| | `immutable::map::builder` | immer's transient |
|---|---|---|
| built one at a time | 88 | 144 |
| find | 20 | 15 |
| an edit, per change | 137 | 103 |
| the edit a version a change | 559 | 407 |

Built one at a time the builder is ahead: a node it made takes the next element where it lies until it is full, and grows into the next size once. The edit is behind: an `erase` looks its key up before it touches anything, so that an absent key copies nothing, which is a second walk; and the first change through a node the map shares copies it, a node with room for one more entry.

The list against `std::forward_list` in place (immer has no list): a million `long`s pushed in front one version each, the million walked, and popped from the front one version each, ns per element:

| | `immutable::list` | `std::forward_list` |
|---|---|---|
| push_front | 13–16 | 11 |
| walk, per cell | 1.5 | 0.4 |
| pop_front | 7 | 8 |

A `push_front` is one managed allocation and one barriered store against a `malloc` and a plain store; a `pop_front` frees nothing, where the mutable list frees a node; the walk is the same chain of dependent loads on both, the cells in the order they were made — the gap there is the load of the next link through a `tracked_ptr`, an atomic load, which keeps the compiler from fetching the element and the link in one instruction, together with the order in which a thread is handed its pages; measured, and left as it is.

What the numbers say. A change copies the nodes on a path, 32 words each; `im` copies them without the write barrier and shades the source node once ([vector](vector.md), [tracked_ptr: shade](../core/tracked_ptr.md#shade-storep-barrieroff)), where immer copies the words and bumps a reference count per child: `set` 131 against 259–429, `push_back` 22 against 25–31, the map's insert level with immer's thread-safe policy. Building at once: `immutable::map` sorts by hash and builds the trie bottom up, 91 against immer's transient's 150; `immutable::vector`'s range constructor fills leaves in order, 4.9 against 2.9–3.5 (a managed allocation per leaf and branch against immer's free list). The reads: the vector reads 3.2 ns against 3.4–4.0. It read 5.2 while its tails and the leaves of its trie were one type, so one pool: every `push_back` copies the tail and drops the copy at the next push, and the few leaves of the trie on each page kept the pages of a thousand dead tails, 73 MB of pages after a collection for a million elements of 8 MB, and a random read touching a page for nearly every element; the tails a push drops lie on pages of their own now, 9 MB, and a walk of the vector is 0.40 ns per element where it was 1.06. The map is behind, 24 against 16, and not in the walk, which is a few instructions a level: `bench_immutable` reads it right after the insertions, while the collector sweeps the 800,000 nodes they dropped, and the nodes the insertions kept lie among them; a map built at once and read after a collection reads as immer's, 16.6.

