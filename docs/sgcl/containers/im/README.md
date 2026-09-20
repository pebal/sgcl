# sgcl::im

The immutable containers: `im::vector`, `im::list`, `im::map`, `im::set`, every operation of which returns a new container and leaves the old one exactly as it was, the two sharing everything but the path that changed. `#include "sgcl/containers/im/im.h"` brings the family in (`sgcl/containers/containers.h` includes it too); it is part of the [containers](../README.md) module, in a directory and a namespace of its own, and uses [`core`](../../core/README.md) and the mutable `vector`; [`concurrent`](../../concurrent/README.md) publishes its versions (`copy_on_write`, `atomic`).; the index of the whole interface is [`docs/sgcl/`](../../README.md); the containers against immer and `std`, in numbers, are on [benchmarks](benchmarks.md).

## The namespace

The containers keep the names of their mutable counterparts — `vector`, `list`, `map`, `set` — and differ by the namespace, `sgcl::im`, two letters as `io`'s (the abbreviation the immer library and the Rust crates use). Code written on them alone says `using namespace sgcl::im;` and reads as any container code; code that mixes the two kinds qualifies, and the qualification is what tells a reader which kind a value is, as `std::pmr::vector` tells against `std::vector`. [`string`](../../core/string.md) is not here: it is immutable already and has no mutable twin, so it stays `sgcl::string`.

## What they are for

A program whose state is a value rather than a place: the state is an `im::map` or an `im::vector`, the next state is a new one made from it, and the old one is still there for whoever holds it. This is what a declarative user interface is built on — `view = f(state)`: whether a subtree needs rebuilding is answered by comparing the roots of the old and the new value, one word each, not by walking the subtree; undo is the list of the old states; a rendering thread reads one version while the logic builds the next, with no lock, because nothing is ever modified. It is also what a configuration read by every thread and replaced by one is, and what a snapshot handed to a task that outlives the change is. The mutable [`vector`](../vector.md) stays the default for everything else: a `push_back` in place is four nanoseconds and a random access one load, where the trie's is a walk of a few nodes.

## The structures

`im::vector<T>`, `im::map<Key, T, Hash, KeyEqual>` and `im::set<Key, Hash, KeyEqual>` are the persistent structures of Clojure's and Scala's standard libraries, which Go and Java do not have: a vector, a map and a set every operation of which returns a new version and leaves the old one exactly as it was, the two sharing everything but the path that changed. The vector is Clojure's bit-partitioned trie, 32-way branches indexed by five bits of the position per level with a tail of up to 32 elements held apart: a random access walks log32(*n*) branches, `set` copies that path and shares the rest, `push_back` copies the tail and hangs it on the trie once in 32 pushes. The map and the set are Bagwell's hash array mapped trie as Clojure's `PersistentHashMap` has it: 32-way nodes indexed by five bits of the hash per level, each storing only the slots in use behind a bitmap, a chain for keys whose hashes agree to the last bit; `insert` and `erase` copy log32(*n*) nodes. `im::list<T>` is the list of Lisp, ML and Elm, older than them all: a chain of cells, `push_front` one cell in front of the chain the new list then shares with the old, `pop_front` the rest of the chain with nothing made — the structure of a stack that keeps its history, and the one whose reference-counted version a long list defeats (a million frees in a chain), which a collector reclaims in one sweep. Immutability is what makes a structure shareable between threads with no synchronization at all, since nothing is ever modified: a version held by any number of threads is read by all of them, and the one place that needs care is the variable that names the current version, which a `copy_on_write<im::map<Key, T>>` or an `atomic<tracked_ptr<...>>` publishes with one load per reader and one compare-exchange per writer. An update through the `copy_on_write` then costs a path of the trie, not a copy of the whole value, which is what `copy_on_write` alone costs and what makes the two compose.

The argument for having them in this library is the collector. Structural sharing means that a node belongs to no version: it is reachable from any number of them, and it may be freed exactly when the last of them lets it go. In a language without a collector that is a reference count per node, incremented and decremented on every copy of a version, atomically when the versions are shared between threads, and it is where the implementations in C++ spend their time and where they get complicated. Here a node is a managed object, a version is a few words holding a `tracked_ptr` to a root, and the question of when a node dies is the collector's, answered the way it is answered for everything else: two versions of a vector of a hundred thousand elements that differ in one element are the one vector plus five objects, and dropping either frees what only it reached. Nodes and leaves are managed objects with destructors, so the elements may be anything, strings, `tracked_ptr`s, objects with destructors, traced and destroyed where they live.

```cpp
sgcl::copy_on_write<sgcl::im::map<sgcl::string, int>> limits;   // the current version, for every thread
// a reader, any thread
auto m = limits.load();                              // one load: this version, immutable, alive while m is
if (auto n = m->find("connections")) { use(*n); }    // a string_view or a literal: no string made
// a writer
limits.update([](auto& m) { m = m.insert("connections", 200); });   // log32(n) nodes copied, the rest shared
// versions as values
sgcl::im::vector<int> v = {1, 2, 3};
auto w = v.push_back(4).set(0, 10);                  // v is still {1, 2, 3}; w shares its leaf with nobody, its branches with v
```

## Pages

| page | header | what it is |
|---|---|---|
| [vector](vector.md) | `sgcl/containers/im/vector.h` | Clojure's bit-partitioned trie with a tail: every `push_back`, `pop_back` and `set` a new version sharing all but a path |
| [list](list.md) | `sgcl/containers/im/list.h` | the list of Lisp and ML: a chain of cells, `push_front` one cell in front of the shared chain, `pop_front` the rest of it; the structure of a history |
| [map](map.md) | `sgcl/containers/im/map.h` | Bagwell's hash array mapped trie: every `insert` and `erase` a new version sharing all but a path; transparent lookup |
| [set](set.md) | `sgcl/containers/im/set.h` | the same trie with the key as the element |
| [benchmarks](benchmarks.md) | | the vector, the list and the map against immer and `std`, one thread, the cost of a version |
