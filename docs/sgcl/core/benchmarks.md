[sgcl](../README.md) › [core](README.md)

# Benchmarks: core

The setup, the machine, the environments and how the timers and the memory are read are described with [the benchmarks of the engine](../../garbage_collector/benchmarks.md); the numbers here were taken the same way.

## Containers
`benchmarks/containers.sh` runs each container case in a process of its own against the `std` counterpart: one million elements, nanoseconds per operation, the footprint of the built container and the peak resident size of the run; the table is the best of three runs of the script per case (19 September 2026, after the review below), and one run differs from the next by a tenth and more in the map rows, the machine's spread at this size. The footprint is what the container occupies once the garbage of building it is gone: for SGCL the pages holding live managed objects after a full collection (free slots left in a page by erasures stay counted, they are reusable by objects of that type), for `std` the bytes malloc reports in use (blocks freed and kept by malloc are not counted). The peak RSS adds what waits for a collection on one side and what malloc keeps on the other.

The node containers cost what `std`'s do or less, the maps within their spread: allocation is the collector's, iteration is a plain load per step, a node is as big as its `std` counterpart and pays no malloc rounding (a 24-byte list node takes 32 bytes from malloc). A `push_back` compares two words of the vector object, stores the element and stores the count; the count is reloaded on the next push (a store the compiler cannot keep in a register across the growth call), which `std::vector` avoids with its end pointer, and a fresh buffer is fresh pages, faulted in on first touch, where `malloc` hands back memory it already touched. The maps pay for the write barriers on the links they relink. The table has one SGCL column: a `sgcl::` container pays the test of the mode on its root word and nothing per element (a `sgcl::vector<sgcl::tracked_ptr<T>>` stores `sgcl::tracked_ptr`s, since an element type that names a `tracked_type` is stored as that type, one word in the same mode inside a managed buffer or node, and hands them out as `sgcl::tracked_ptr<T>&`), and the `sgcl::` variant of `bench_containers` measures within the run-to-run spread of the `sgcl::` one.

| Case | SGCL ns | `std` ns | SGCL footprint | `std` footprint | SGCL peak | `std` peak |
|---|---|---|---|---|---|---|
| vector push_back | 2.6 | 1.5 | 8.8 MB | 8.0 MB | 18 MB | 17 MB |
| vector of pointers, copy and walk | 2.6 | 8.7 (`shared_ptr`) | 16.4 MB | 31.3 MB | 42 MB | 64 MB |
| deque push both ends | 1.8 | 1.3 | 8.9 MB | 7.7 MB | 11 MB | 9 MB |
| list push_back / iterate | 15.8 / 1.9 | 25.6 / 1.9 | 23.0 MB | 30.5 MB | 27 MB | 32 MB |
| list erase every other | 18.0 | 26.0 | 23.0 MB | 15.3 MB | 27 MB | 32 MB |
| forward_list push_front | 12.8 | 21.3 | 15.4 MB | 15.3 MB | 19 MB | 17 MB |
| map insert / find / iterate | 337 / 255 / 120 | 342 / 265 / 121 | 45.9 MB | 45.8 MB | 58 MB | 55 MB |
| set insert | 272 | 311 | 38.2 MB | 45.8 MB | 50 MB | 55 MB |
| map insert / find | 131 / 172 | 123 / 182 | 39.3 MB | 43.1 MB | 83 MB | 65 MB |
| map erase half | 309 | 318 | 39.3 MB | 27.8 MB | 72 MB | 65 MB |
| map of pointers | 75 | 118 (`shared_ptr`) | 47.0 MB | 88.9 MB | 91 MB | 111 MB |

## Strings

What it costs, in nanoseconds per operation, against a `std::string` member, Go's string and Java's `String` (`benchmarks/core/string.cpp` and its Go and Java counterparts, the setup of [Containers](#containers), one thread): 2 M strings of 10 and of 100 characters made from a text buffer and stored in nodes, copied from node to node, hashed once each as a map key would be, and hashed eight times in a row (a key used again and again); `sgcl::string` in the same managed nodes:

| Operation, length | `sgcl::string` | `std::string` | Go string | Java `String` |
|---|---|---|---|---|
| make, 10 | 10.2 | 2.9 | 10.1 | 23.3 |
| make, 100 | 18.0 | 20.6 | 22.3 | 32.4 |
| copy, 10 | 1.9 | 1.9 | 1.3 | 1.1 |
| copy, 100 | 1.9 | 24.0 | 1.4 | 6.6 |
| hash once, 10 | 2.8 | 1.8 | 5.4 | 5.0 |
| hash once, 100 | 12.3 | 11.4 | 8.7 | 14.6 |
| hash 8 times, 10 | 0.9 | 1.8 | 5.5 | 0.9 |
| hash 8 times, 100 | 2.3 | 9.4 | 7.3 | 1.9 |

Below its small buffer (22 characters in libc++, 15 in libstdc++ and MSVC) a `std::string` costs no allocation, and no type that allocates can match that: a `string` of a few characters costs a managed allocation to make ([Allocation](../../garbage_collector/how-it-works.md#allocation)), as a Go string does. Past the buffer, a `std::string` costs an allocation to make, an allocation and a copy per copy (24 ns for 100 characters), and a `free` in the sweep for each; a `string` costs the same allocation once, a word and the barrier per copy (1.9 ns, whatever the length, the same as a small `std::string`'s 24 bytes), and nothing in the sweep. A copy in Go is two words without a barrier while the collector is idle; in Java a reference through ZGC's store barrier. The first hash reads the characters everywhere (the `string`'s is `std::hash` of the characters, plus the store that keeps it: a nanosecond over `std::string`'s); the `string` and Java's `String` keep it in the object and pay a load from then on (the eight-times row: one computation and seven loads), where `std::string` and Go read the characters every time. What the table does not show is the sweep: two million nodes with 100-character strings freed in 31 ms with `string`s and 46 ms with `std::string`s, whose buffers `free` returns one by one, and the other way round for ten characters (24 ms against 17: two million more objects, where the `std::string`s lay inside their nodes). So `std::string` remains the member for text of a few characters made and dropped, and `string` is the member for text that is kept, shared and compared: names, keys, symbols, the leaves of a document.

## Text

The hash of a string is keyed (four words drawn once per process) and faster than the standard library's unkeyed
hash at every length: 2.4 ns for three bytes, 4.2 ns for a hundred, 10 GB/s over a long text; a string computes it
once and keeps it ([string](string/README.md)).

The questions about a text stand on `utf8::ascii_run`, which reads a run of ASCII eight bytes at a time
([utf8](utf8/README.md)). `utf8::count`, what `rune_count()` is, reads 0.04 ns a byte over ASCII, where it read 0.60 a code
point at a time and Go's `utf8.RuneCountInString` reads 0.34; `to_lower()` and `to_upper()` of a string read 0.11 ns
a byte over ASCII, where they read 4.4 decoding a code point at a time.

## Weak pointers

Nanoseconds per operation against `std::weak_ptr`, Go's `weak.Pointer` and Java's `WeakReference`, one thread and four threads on objects of their own (`benchmarks/core/weak_ptr.cpp` and its Go and Java counterparts, the setup of [Containers](#containers)):

| Operation, threads | `sgcl::weak_ptr` | `std::weak_ptr` | Go `weak.Pointer` | Java `WeakReference` |
|---|---|---|---|---|
| lock, 1 | 1.8 | 12.1 | 6.1 | 1.0 |
| lock, 4 | 1.9 | 12.7 | 6.2 | 1.1 |
| lock of an expired pointer, 1 | 0.9 | 1.4 | 5.2 | 1.0 |
| lock of an expired pointer, 4 | 1.0 | 1.5 | 4.0 | 1.1 |
| copy, 1 | 1.4 | 8.6 | 6.3 | 0.8 |
| copy, 4 | 1.5 | 8.8 | 6.4 | 0.9 |
| make from a strong pointer, 1 | 7.4 | 8.5 | 18.4 | 3.6 |
| make from a strong pointer, 4 | 7.8 | 8.8 | 18.7 | 7.2 |

A lock is the cell read twice around a hazard store and a `tracked_ptr` built; on an expired pointer the same reads find the null and build a null pointer; a copy is a `tracked_ptr` copy; making one is a 16-byte allocation. `std::weak_ptr` pays two atomic count updates per lock and per copy, and contended ones when threads share an object. Go's `weak.Pointer` is a handle the runtime hands out and resolves in a call (`Value`), and making one allocates the handle; Java's `WeakReference` is an object whose referent is read through ZGC's load barrier, the cheapest lock of the four, and making one is an allocation the collector has to discover.

## The review of September 2026

The containers were reviewed whole on 19 September 2026 and the findings fixed in one pass; the ones that changed a cost were measured apart, with probes of the same shape as the matrix (`-O2`, a million or two hundred thousand elements, the headers before and after in the same probe), since none of them is a row of the table:

| What | Before | After |
|---|---|---|
| `sorted_map<long, long>` copied, 1,000,000 keys, per element | 96 to 99 ns | 58 to 62 ns |
| `sorted_map<std::string, int>` copied, 200,000 keys, per element | 58 to 60 ns | 24 to 27 ns |
| `map<std::string, int>` built from a range of 200,000 `std::pair<std::string, int>`, per element | 135 to 169 ns | 73 to 95 ns |
| `weak_map<Node, int>` insertion of a new object | 81 to 101 ns | 76 to 94 ns |
| `ordered_map` `to_back` of the element already last (an LRU touching its newest) | 19.6 to 20.3 ns | 10.6 ns |
| `deque<long>` of 1,000,000, `operator[]` in a loop, per element | 0.51 ns | 0.30 ns |
| `vector<int>` of 20,000, 2000 × `resize(size() + 1)` | 2000 reallocations | 1 |

The copy of a map inserted element by element, with a comparison and the rebalancing per node; it copies the tree shape for shape now, a node per element with its colour and its links, no comparison and no rebalancing ([sorted_map](sorted_map/sorted_map.md)). A range of `std::pair<Key, T>` into a hash map converted each pair to the element type twice before the node copied it a third time; the key is hashed and looked up where it is now and copied once ([map](map/insert.md)). The weak containers searched twice per insertion, once to look and once inside the table's own insert, and built the value on the stack before moving it in; one search, the value built in place on a miss ([weak_map](weak_map/emplace.md)). `to_back` relinked an element already at the back, eight stores with the write barrier on the LRU's most frequent hit; a load and nothing more now ([ordered_map](ordered_map/to_back.md)). The deque read its own map and blocks with the atomic load of a `tracked_ptr`, which the compiler cannot hoist out of a loop; the plain load a container uses on its own buffer now, except in `emplace_back`'s fast path, where the plain load made the compiler emit a slower loop (measured, kept as it was). `resize(n)` reserved exactly `n`, so a resize by one at a time reallocated every time; it grows as a push grows now ([vector](vector/README.md#modifiers)). What the review fixed without a cost to show is on the pages: the erase paths of the lists write nothing to a node after its element is destroyed and hold every node through its own destructor, a node handle dying in a sweep leaves its node to the sweep, `deque::erase` on an empty deque, `vector::insert` within the capacity under an exception, the constexpr of `array<T, N>`, the range constructors of the tree from a range of another type, `merge` across the hash tables' two node layouts, the bound of `weak_multimap::erase(iterator)`, the move assignment of `expiry_queue`.
