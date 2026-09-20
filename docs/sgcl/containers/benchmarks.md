# Benchmarks: the containers

The setup, the machine, the environments and how the timers and the memory are read are described with [the benchmarks of the engine](../../garbage_collector/benchmarks.md); the numbers here were taken the same way. The cost of a [string](README.md#string) is measured on its page.

## Containers
`benchmarks/containers.sh` runs each container case in a process of its own against the `std` counterpart: one million elements, nanoseconds per operation, the footprint of the built container and the peak resident size of the run; the table is the best of three runs of the script per case (19 September 2026, after the review below), and one run differs from the next by a tenth and more in the map rows, the machine's spread at this size. The footprint is what the container occupies once the garbage of building it is gone: for SGCL the pages holding live managed objects after a full collection (free slots left in a page by erasures stay counted, they are reusable by objects of that type), for `std` the bytes malloc reports in use (blocks freed and kept by malloc are not counted). The peak RSS adds what waits for a collection on one side and what malloc keeps on the other.

The node containers cost what `std`'s do or less, the maps within their spread: allocation is the collector's, iteration is a plain load per step, a node is as big as its `std` counterpart and pays no malloc rounding (a 24-byte list node takes 32 bytes from malloc). A `push_back` compares two words of the vector object, stores the element and stores the count; the count is reloaded on the next push (a store the compiler cannot keep in a register across the growth call), which `std::vector` avoids with its end pointer, and a fresh buffer is fresh pages, faulted in on first touch, where `malloc` hands back memory it already touched. The maps pay for the write barriers on the links they relink. The table has one SGCL column: a `sgcl::` container pays the test of the mode on its root word and nothing per element (a `sgcl::vector<sgcl::tracked_ptr<T>>` stores `sgcl::tracked_ptr`s, since an element type that names a `tracked_type` is stored as that type, one word in the same mode inside a managed buffer or node, and hands them out as `sgcl::tracked_ptr<T>&`), and the `sgcl::` variant of `bench_containers` measures within the run-to-run spread of the `sgcl::` one.

| case | SGCL ns | `std` ns | SGCL footprint | `std` footprint | SGCL peak | `std` peak |
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

## The review of September 2026

The containers were reviewed whole on 19 September 2026 and the findings fixed in one pass; the ones that changed a cost were measured apart, with probes of the same shape as the matrix (`-O2`, a million or two hundred thousand elements, the headers before and after in the same probe), since none of them is a row of the table:

| what | before | after |
|---|---|---|
| `sorted_map<long, long>` copied, 1,000,000 keys, per element | 96 to 99 ns | 58 to 62 ns |
| `sorted_map<std::string, int>` copied, 200,000 keys, per element | 58 to 60 ns | 24 to 27 ns |
| `map<std::string, int>` built from a range of 200,000 `std::pair<std::string, int>`, per element | 135 to 169 ns | 73 to 95 ns |
| `weak_map<Node, int>` insertion of a new object | 81 to 101 ns | 76 to 94 ns |
| `ordered_map` `to_back` of the element already last (an LRU touching its newest) | 19.6 to 20.3 ns | 10.6 ns |
| `deque<long>` of 1,000,000, `operator[]` in a loop, per element | 0.51 ns | 0.30 ns |
| `vector<int>` of 20,000, 2000 × `resize(size() + 1)` | 2000 reallocations | 1 |

The copy of a map inserted element by element, with a comparison and the rebalancing per node; it copies the tree shape for shape now, a node per element with its colour and its links, no comparison and no rebalancing ([sorted_map](sorted_map.md#constructors)). A range of `std::pair<Key, T>` into a hash map converted each pair to the element type twice before the node copied it a third time; the key is hashed and looked up where it is now and copied once ([map](map.md#insert)). The weak containers searched twice per insertion, once to look and once inside the table's own insert, and built the value on the stack before moving it in; one search, the value built in place on a miss ([weak_map](weak_map.md#emplace-insert-insert_or_assign)). `to_back` relinked an element already at the back, eight stores with the write barrier on the LRU's most frequent hit; a load and nothing more now ([ordered_map](ordered_map.md#to_back-to_front)). The deque read its own map and blocks with the atomic load of a `tracked_ptr`, which the compiler cannot hoist out of a loop; the plain load a container uses on its own buffer now, except in `emplace_back`'s fast path, where the plain load made the compiler emit a slower loop (measured, kept as it was). `resize(n)` reserved exactly `n`, so a resize by one at a time reallocated every time; it grows as a push grows now ([vector](vector.md#resize)). What the review fixed without a cost to show is on the pages: the erase paths of the lists write nothing to a node after its element is destroyed and hold every node through its own destructor, a node handle dying in a sweep leaves its node to the sweep, `deque::erase` on an empty deque, `vector::insert` within the capacity under an exception, the constexpr of `array<T, N>`, the range constructors of the tree from a range of another type, `merge` across the hash tables' two node layouts, the bound of `weak_multimap::erase(iterator)`, the move assignment of `expiry_queue`.
