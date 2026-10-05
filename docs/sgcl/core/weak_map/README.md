[sgcl](../../README.md) › [core](../README.md)

# sgcl::weak_map\<Key, T\>

```cpp
#include "sgcl/core/weak_map.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class Key, class T>
    class weak_map;
}
```

**Requires [rooted](../rooted/README.md) outside a stack or a managed object.**

`sgcl::weak_map<Key, T>` maps objects to values without keeping the objects alive. The key is the object itself,
its identity and not its contents: an entry is looked up, made and erased by a `tracked_ptr<Key>` to the object,
and held by a [weak_ptr](../weak_ptr/README.md). An entry whose object the collector has found unreachable is dead: never
found, passed over by the iteration, dropped by a sweep. Metadata attached to objects from outside, a cache keyed by
the object, a registry that forgets. [weak_multimap](../weak_multimap/README.md) holds several values per object,
[weak_set](../weak_set/README.md) the objects alone, and [concurrent::weak_map](../../concurrent/weak_map/README.md) is the map shared by
threads without a lock.

The entries are hashed and compared by the object's address, which the weak pointer's cell holds while the object
lives and the collector clears before the address can be handed out again (the weak phase runs before the sweep
that frees the slot: [weak_ptr](../weak_ptr/README.md)). So a dead entry equals nothing, its own key included, and can neither
be found nor block the entry of the object that takes the slot next. Dead entries are swept out every so many
insertions, as many as the map has entries, so that a pass costs less than the insertions that paid for it, and on
[sweep](sweep.md); [size](size.md) counts the entries a sweep has not yet dropped.

The values are the map's own, destroyed with the entry. A value holding a strong pointer to its own key keeps the
key alive, and the entry with it: the map has no ephemerons. A `sorted_multimap<const Node*, …>` would not do: a raw
address in a managed container is a word holding a heap address, which the pointer map built by elimination
([Pointer maps](../../../garbage_collector/overview.md#pointer-maps)) follows like a `tracked_ptr`, so the map would
keep every object alive by its key.

What differs from `std::unordered_map`: the key is the object, compared by identity and given apart from the value
(`emplace(object, a...)`, not a pair); `emplace` searches first and builds nothing when the object has an entry, as
`try_emplace` does; the iteration hands out the object, held, with a reference to its value; there is no `at`, and
the map is moved but not copied. What differs from Java's `WeakHashMap`: the key is compared by identity, not by
`equals`, as in Java's `IdentityHashMap`. Go's library has no weak map.

## Rules

- The values may be, or hold, tracked pointers: the nodes are managed objects. A value that reaches its own key
  keeps the key, and so the entry, alive for as long as the entry is in the map.
- An entry is dead once a cycle has found its object unreachable; between the object becoming unreachable and that
  cycle, it is found and visited like any other: the lag of any garbage collector.
- The iteration hands out the object as a strong pointer, held while the iterator stands on the entry: the entry cannot
  die under it. An iterator is a tracked object then.
- The value of an entry is stable while the entry is in the map; a reference to it is invalid once the entry is
  erased or swept, as in `std::unordered_map`.
- Thread safety is that of `std::unordered_map`: concurrent readers, or one writer, with the program's own
  synchronization ([The rules](../README.md#the-rules), 6). The collector clearing a key's cell at the same time is
  safe.

## Template parameters

| Parameter | Description |
|---|---|
| `Key` | The type of the objects: any type a `tracked_ptr` points to, a class, an abstract base, `int`. It is neither hashed nor compared: the key is the object's identity. |
| `T` | The type of the values: any object type constructible from the arguments of the insertion; it need not be copyable or movable, as `emplace` and `operator[]` construct it in the entry. It may be, or hold, tracked pointers. |

## Member types

| Type | Definition |
|---|---|
| `key_type` | `Key` |
| `key_pointer` | `tracked_ptr<Key>` |
| `mapped_type` | `T` |
| `weak_type` | `weak_ptr<Key>` |
| `size_type` | `size_t` |
| `reference` | a struct of the library with the members `key_pointer key` and `T& value`: what an iterator gives out, the object, held, and its value |
| `const_reference` | the same with `const T& value`: what a `const_iterator` gives out |
| `iterator` | a forward iterator over the live entries, of a class of the library; `*it` is a `reference`, `it->key`, `it->value` |
| `const_iterator` | the same over `const_reference`; an `iterator` converts to it |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](weak_map.md) | constructs an empty map, or takes one over |
| `(destructor)` | destroys the values; the nodes are left to the collector |
| [operator=](operator_assign.md) | takes the entries of another map over |

#### Element access

| Function | Description |
|---|---|
| [operator[]](operator_at.md) | the value of an object, made when the object has none |

#### Iterators

| Function | Description |
|---|---|
| [begin, cbegin](begin.md) | an iterator to the first live entry |
| [end, cend](end.md) | an iterator past the last entry |

#### Capacity

| Function | Description |
|---|---|
| [empty](empty.md) | checks whether the map holds an entry, a dead one included |
| [size](size.md) | the number of entries, the dead ones not yet swept included |

#### Modifiers

| Function | Description |
|---|---|
| [clear](clear.md) | erases every entry |
| [insert](insert.md) | inserts a value for an object, unless the object has one |
| [insert_or_assign](insert_or_assign.md) | inserts a value for an object, or assigns it to the one there |
| [emplace](emplace.md) | constructs a value for an object in place, unless the object has one |
| [erase](erase.md) | erases the entry of an object, or the one an iterator stands on |
| [sweep](sweep.md) | erases the entries whose objects are gone |

#### Lookup

| Function | Description |
|---|---|
| [count](count.md) | the number of entries of an object, 0 or 1 |
| [find](find.md) | the entry of an object |
| [contains](contains.md) | checks whether an object has an entry |

## Complexity

- `find`, `contains`, `count`, `erase`: constant on average.
- `insert`, `emplace`, `insert_or_assign`, `operator[]`: constant on average, plus, every so many insertions, a
  sweep linear in the number of entries: amortized constant, as the sweep comes after as many insertions as the map
  had entries after the last one, 16 at least.
- `sweep`, `clear`: linear in the number of entries. `size`, `empty`, `end`: constant. `begin`: constant, plus the
  dead entries before the first live one, which it passes.

## Iterator invalidation

| Operations | Invalidated |
|---|---|
| all read-only operations, `insert`, `emplace`, `insert_or_assign`, `operator[]`, `sweep` | never: an iterator stands on a live entry and holds its object, so no sweep drops its entry, and a growth of the table keeps every node where it is |
| `erase` | the iterators to the erased entry |
| `clear`, `operator=` | always |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Node {
    int id;
};

void name_a_temporary(weak_map<Node, string>& names) {
    tracked_ptr node = make_tracked<Node>(2);
    names[node] = "temporary";
}

int main() {
    // A name for any node, without owning one: the entry dies with its node
    weak_map<Node, string> names;
    tracked_ptr root = make_tracked<Node>(1);
    names[root] = "root";
    name_a_temporary(names);
    println("{} entries", names.size());

    collector::clear_stack();  // the dead frame zeroed: nothing on the stack keeps the node
    collector::force_collect(true);  // optional, for the demonstration
    for (auto [node, name] : names) {
        println("{}: {}", node->id, name);
    }
    size_t swept = names.sweep();
    println("{} swept, {} left", swept, names.size());
}
```

Output:

```text
2 entries
1: root
1 swept, 1 left
```

## See also

- [weak_multimap](../weak_multimap/README.md): several values per object
- [weak_set](../weak_set/README.md): the objects alone; [weak_ptr](../weak_ptr/README.md): the key
- [expiry_queue](../expiry_queue/README.md): a function called with the object when it is found unreachable, for the cleanup
  that needs the object
- [map](../map/README.md): the table underneath
- [concurrent::weak_map](../../concurrent/weak_map/README.md): the map shared by threads without a lock
- README: [Weak containers](../README.md#weak-containers) under [Weak pointers](../weak_ptr/README.md),
  [The rules](../README.md#the-rules)
- `tests/containers/weak_map.cpp`: every behaviour above, checked.
