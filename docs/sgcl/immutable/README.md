[sgcl](../README.md) › immutable

# sgcl::immutable

```cpp
#include "sgcl/immutable.h"   // namespace sgcl::immutable
```

The immutable containers: `immutable::vector`, `immutable::list`, `immutable::map` and `immutable::set`, every
operation of which returns a new container and leaves the old one exactly as it was, the two sharing everything
but the path that changed. They are the persistent structures of Clojure's and Scala's standard libraries, which
Go and Java do not have, for a program whose state is a value rather than a place: the state is a map or a
vector, the next state is a new one made from it, and the old one is still there for whoever holds it. A
declarative user interface is built on that (`view = f(state)`: whether a subtree needs rebuilding is answered by
comparing the roots of the old and the new value, one word each); undo is the list of the old states; a rendering
thread reads one version while the logic builds the next, with no lock, because nothing is ever modified. So is
a configuration read by every thread and replaced by one, and a snapshot handed to a task that outlives the
change. The mutable [vector](../core/vector/README.md) stays the default for everything else.

The argument for having them in this library is the collector. Structural sharing means that a node belongs to no
version: it is reachable from any number of them, and it may be freed exactly when the last of them lets it go.
In a language without a collector that is a reference count per node, incremented and decremented on every copy
of a version, atomically when the versions are shared between threads, and it is where the implementations in C++
spend their time and where they get complicated. Here a node is a managed object, a version is a few words holding
a `tracked_ptr` to a root, and the question of when a node dies is the collector's, answered the way it is
answered for everything else: two versions of a vector of a hundred thousand elements that differ in one element
are the one vector plus a path of a few objects, and dropping either frees what only it reached. The nodes have
destructors, so the elements may be anything: strings, `tracked_ptr`s, objects with destructors, traced and
destroyed where they live.

The containers keep the names of their mutable counterparts and differ by the namespace, `sgcl::immutable`, as
`std::pmr::vector` differs from `std::vector`: code written on them alone says `using namespace sgcl::immutable;`
and reads as any container code, and code that mixes the two kinds qualifies, which tells a reader which kind a
value is. [string](../core/string/README.md) is not here: it is immutable already and has no mutable twin, so it stays
`sgcl::string`. The module depends on [core](../core/README.md) alone; [concurrent](../concurrent/README.md)
publishes its versions.

## The rules

1. A container of the module holds its root by a `tracked_ptr`, so it lives where one may: on a thread's stack or
   inside a managed object ([core: The rules](../core/README.md#the-rules), 1). A copy of it is a copy of a few
   words.
2. Every member is `const` but the assignment. A change (`push_back`, `set`, `insert`, `erase`...) returns the new
   container; the one it was called on is unchanged, and stays so for as long as it is held. The elements are
   reached as `const`.
3. A change of a vector, a map or a set copies the elements of the nodes on its path, up to 32 per node: the copy
   constructor of the element is what a change costs, plus the nodes. A list's `push_front` makes one cell and
   `pop_front` nothing. Nothing is destroyed by a change: the old version still holds what the new one dropped.
4. An element holding tracked pointers is traced where it lives, in a node; an element with a destructor is
   destroyed when the collector frees its node, once no version reaches it.
5. An iterator, and a pointer or a reference to an element, holds nothing alive: it is valid while the container
   it came from holds the version it was taken from, as with `std`.
6. Any number of threads read any version without synchronization. The one place that needs care is the variable
   that names the current version: a `concurrent::copy_on_write<immutable::map<Key, T>>` or an `atomic` publishes
   it with one load per reader and one compare-exchange per writer. An update through the `copy_on_write` then
   costs a path of the trie, not a copy of the whole value, which is what `copy_on_write` alone costs and what
   makes the two compose.
7. A builder ([map::builder](map-builder/README.md), [set::builder](set-builder/README.md)) is the one object of the module
   changed in place: one thread's, moved but never copied, and it never changes a container it was thawed from or
   froze.

## Containers

| Container | Header | Description |
|---|---|---|
| [list\<T\>](list/README.md) | `list.h` | the list of Lisp and ML: a chain of cells, `push_front` one cell in front of the shared chain, `pop_front` the rest of it; the structure of a history |
| [map\<Key, T, Hash, KeyEqual\>](map/README.md) | `map.h` | Bagwell's hash array mapped trie: every `insert`, `set` and `erase` a new version sharing all but a path; transparent lookup |
| [map\<Key, T, Hash, KeyEqual\>::builder](map-builder/README.md) | `map.h` | a map changed in place and frozen into a map: `thaw`, `freeze`, for many changes at once |
| [set\<Key, Hash, KeyEqual\>](set/README.md) | `set.h` | the same trie with the key as the element |
| [set\<Key, Hash, KeyEqual\>::builder](set-builder/README.md) | `set.h` | a set changed in place and frozen into a set |
| [vector\<T\>](vector/README.md) | `vector.h` | Clojure's bit-partitioned trie with a tail: every `push_back`, `pop_back` and `set` a new version sharing all but a path |

The containers carry the mixins of core ([the mixins](../core/mixin/README.md)): the questions of
[mixin::enumerable](../core/mixin/enumerable/README.md) all of them, the order of
[mixin::ordered](../core/mixin/ordered/README.md) the vector and the list, the reads by the key of
[mixin::lookup](../core/mixin/lookup/README.md) the map, and the declaration [mixin::immutable](../core/mixin/immutable.md)
all four, which [req::immutable](../core/req/immutable.md) asks for.

## See also

- [Benchmarks](benchmarks.md): the vector, the list and the map against immer and `std`
- [concurrent::copy_on_write](../concurrent/copy_on_write/README.md): how a version is published to other threads
- [core: Containers](../core/README.md#containers): the mutable containers
- [The modules](../README.md)
