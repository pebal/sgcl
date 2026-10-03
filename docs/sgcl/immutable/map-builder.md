[sgcl](../README.md) › [immutable](README.md) › [map](map.md)

# sgcl::immutable::map\<Key, T, Hash, KeyEqual\>::builder

```cpp
#include "sgcl/immutable/map.h"   // or "sgcl/immutable.h"

namespace sgcl::immutable {
    template<class Key, class T, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
    class map {
    public:
        class builder;
    };
}
```

`sgcl::immutable::map<Key, T, Hash, KeyEqual>::builder` is a map changed in place, one element at a time, and
frozen into a [map](map.md) when it is done: the transient of Clojure and immer. [thaw()](map/thaw.md) is the
builder over a map's trie, which the two share, and copies nothing. An `insert` or an `erase` changes in place the
nodes the builder made, and a node it shares with a map it copies once, the first time a change goes through it:
where the map's own `insert` copies the whole path every time and leaves the old one to the collector, the builder
copies a node once and changes it after.

[freeze()](map-builder/freeze.md) hands out the map of what the builder holds, and the builder goes on; its next
change copies again whatever the map now shares, so the map is a value like any other and nothing the builder does
afterwards reaches it.

Nothing needs a builder: a map built at once from a range is the [constructor](map/map.md)'s, and about as fast.
The builder is for a map made or changed one element at a time, with conditions between, or for many changes to a
large map at once.

## Rules

- A builder is one thread's, and it moves but does not copy: two builders would change one node. A builder moved
  from is empty.
- A builder holds the root of its trie by a `tracked_ptr`, so it lives where one may: on a thread's stack or
  inside a managed object ([The rules](../core/README.md#the-rules), 1).
- A builder has no iterators: its nodes change under them. [try_get](map-builder/try_get.md) hands back a pointer
  valid until the builder's next change, which may move the element within its node.
- An element added or taken out moves the elements after it in its node, which needs a move of
  `pair<const Key, T>` that cannot throw. A key whose copy can throw (a `std::string`: the key is `const` in the
  pair, so it is copied, not moved) makes the builder copy that one node instead of moving its elements, and
  still never the path above it.
- A map the builder froze, or was thawed from, is never changed by it.

## Template parameters

Those of [map](map.md#template-parameters).

## Member functions

| Function | Description |
|---|---|
| [(constructor)](map-builder/map-builder.md) | constructs an empty builder, or takes another over |
| `(destructor)` | drops the builder; its nodes no map holds are left to the collector |
| [operator=](map-builder/operator_assign.md) | takes another builder over |

#### Capacity

| Function | Description |
|---|---|
| [empty](map-builder/empty.md) | checks whether the builder is empty |
| [size](map-builder/size.md) | the number of elements |

#### Lookup

| Function | Description |
|---|---|
| [contains](map-builder/contains.md) | checks whether the builder has an element under a key |
| [try_get](map-builder/try_get.md) | a pointer to the value under a key, null when absent |

#### Modifiers

| Function | Description |
|---|---|
| [insert](map-builder/insert.md) | adds an element when its key is absent |
| [set](map-builder/set.md) | puts a value under a key, added or in place of the value there |
| [emplace](map-builder/emplace.md) | adds an element constructed in place when its key is absent |
| [erase](map-builder/erase.md) | takes out the element under a key |
| [freeze](map-builder/freeze.md) | the map of what the builder holds |

## Complexity

- `insert`, `set`, `emplace`, `erase`: logarithmic in the size, base 32: a walk down the trie, a node copied the
  first time a change goes through it, in place after.
- `freeze`: linear in the number of nodes the builder made since it was thawed or last froze, at most.
- `try_get`, `contains`: as the map's.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // a map made one element at a time, with a condition between
    immutable::map<int, int>::builder squares;
    for (int i : range(1, 11)) {
        if (i % 3 != 0) {
            squares.insert(i, i * i);
        }
    }
    immutable::map<int, int> first = squares.freeze();

    // the builder goes on; the map it froze does not change
    squares.erase(1);
    squares.set(2, -4);
    immutable::map<int, int> second = squares.freeze();
    println("{} {} {}", first.size(), first.at(2), first.contains(1));
    println("{} {} {}", second.size(), second.at(2), second.contains(1));
}
```

Output:

```text
7 4 true
6 -4 false
```

## See also

- [map::thaw](map/thaw.md): a builder over a map
- [set::builder](set-builder.md): the builder of a set
- [sgcl::immutable::map\<Key, T, Hash, KeyEqual\>](map.md)
- [Benchmarks: The builder](benchmarks.md#the-builder)
