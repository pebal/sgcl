[sgcl](../../README.md) › [immutable](../README.md) › [set](../set/README.md)

# sgcl::immutable::set\<Key, Hash, KeyEqual\>::builder

```cpp
#include "sgcl/immutable/set.h"   // or "sgcl/immutable.h"

namespace sgcl::immutable {
    template<class Key, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
    class set {
    public:
        class builder;
    };
}
```

`sgcl::immutable::set<Key, Hash, KeyEqual>::builder` is a set changed in place, one element at a time, and frozen
into a [set](../set/README.md) when it is done, as the [map's builder](../map-builder/README.md) is for a map. [thaw()](../set/thaw.md)
is the builder over a set's trie, which the two share, and copies nothing; an `insert` or an `erase` changes the
nodes the builder made in place and copies a node it shares once, the first time a change goes through it.
[freeze()](freeze.md) is the set of what it holds, and the builder goes on.

## Rules

- A builder is one thread's, and it moves but does not copy: two builders would change one node. A builder moved
  from is empty.
- A builder holds the root of its trie by a `tracked_ptr`, so it lives where one may: on a thread's stack or
  inside a managed object ([The rules](../../core/README.md#the-rules), 1).
- A builder has no iterators and no `find`: its nodes change under them.
- An element added or taken out moves the elements after it in its node when the move of `Key` cannot throw;
  otherwise the builder copies that one node, and still never the path above it.
- A set the builder froze, or was thawed from, is never changed by it.

## Template parameters

Those of [set](../set/README.md#template-parameters).

## Member functions

| Function | Description |
|---|---|
| [(constructor)](set-builder.md) | constructs an empty builder, or takes another over |
| `(destructor)` | drops the builder; its nodes no set holds are left to the collector |
| [operator=](operator_assign.md) | takes another builder over |

#### Capacity

| Function | Description |
|---|---|
| [empty](empty.md) | checks whether the builder is empty |
| [size](size.md) | the number of elements |

#### Lookup

| Function | Description |
|---|---|
| [contains](contains.md) | checks whether the builder has an element equal to a key |

#### Modifiers

| Function | Description |
|---|---|
| [insert](insert.md) | adds an element when no equal one is there |
| [erase](erase.md) | takes out the element equal to a key |
| [freeze](freeze.md) | the set of what the builder holds |

## Complexity

- `insert`, `erase`: logarithmic in the size, base 32: a walk down the trie, a node copied the first time a change
  goes through it, in place after.
- `freeze`: linear in the number of nodes the builder made since it was thawed or last froze, at most.
- `contains`: as the set's.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // the multiples of 3 and of 5 below 30, one at a time
    immutable::set<int>::builder b;
    for (int i : range(30)) {
        if (i % 3 == 0 || i % 5 == 0) {
            b.insert(i);
        }
    }
    immutable::set<int> multiples = b.freeze();
    b.erase(0);
    println("{} {} {}", multiples.size(), multiples.contains(0), b.size());
}
```

Output:

```text
14 true 13
```

## See also

- [set::thaw](../set/thaw.md): a builder over a set
- [map::builder](../map-builder/README.md): the builder of a map
- [sgcl::immutable::set\<Key, Hash, KeyEqual\>](../set/README.md)
