[sgcl](../../README.md) › [core](../README.md)

# sgcl::weak_multimap\<Key, T\>

```cpp
#include "sgcl/core/weak_map.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class Key, class T>
    class weak_multimap;
}
```

**Requires [rooted](../rooted/README.md) outside a stack or a managed object.**

`sgcl::weak_multimap<Key, T>` is the [weak_map](../weak_map/README.md) with several values per object: it maps objects to
values without keeping the objects alive, and an object may have as many entries as the program gives it. The key is
the object itself, its identity and not its contents: an entry is made, looked up and erased by a `tracked_ptr<Key>`
to the object, and held by a [weak_ptr](../weak_ptr/README.md). An entry whose object the collector has found unreachable is
dead: never found, passed over by the iteration, dropped by a sweep. Metadata attached to objects from outside, as
many notes as each needs; the listeners of an object, the tags of a node.

Hashing, equality, the sweeps and the rules are those of `weak_map`: the entries are hashed and compared by the
object's address, which the collector clears from the weak pointer's cell before the address can be handed out
again, so a dead entry equals nothing; dead entries are swept out every so many insertions, as many as the map has
entries, and on [sweep](sweep.md); [size](size.md) counts the entries a sweep has not
yet dropped. The values are the map's own, destroyed with the entry; a value holding a strong pointer to its own key
keeps the key alive, and the entries with it.

What differs from `weak_map`: [emplace](emplace.md) and [insert](insert.md) add one more
value and return its iterator; the entries of an object stand together, the newest first, as in
[multimap](../multimap/README.md); [find](find.md) is the first entry of the object,
[equal_range](equal_range.md) the range of its entries, [count](count.md) counts them and
[erase](erase.md) by the object drops them all; there is no `operator[]` and no `insert_or_assign`, an
object having no one value. What differs from `std::unordered_multimap`: the key is the object, compared by identity
and given apart from the value; the iteration hands out the object, held, with a reference to its value; the map is
moved but not copied.

## Rules

- The values may be, or hold, tracked pointers: the nodes are managed objects. A value that reaches its own key
  keeps the key, and so its entries, alive for as long as the entry is in the map.
- An entry is dead once a cycle has found its object unreachable; between the object becoming unreachable and that
  cycle, it is found and visited like any other: the lag of any garbage collector.
- The iteration hands out the object as a strong pointer, held while the iterator stands on the entry: the entry cannot
  die under it. An iterator is a tracked object then.
- The value of an entry is stable while the entry is in the map; a reference to it is invalid once the entry is
  erased or swept, as in `std::unordered_multimap`.
- Thread safety is that of `std::unordered_multimap`: concurrent readers, or one writer, with the program's own
  synchronization ([The rules](../README.md#the-rules), 6). The collector clearing a key's cell at the same time is
  safe.

## Template parameters

| Parameter | Description |
|---|---|
| `Key` | The type of the objects: any type a `tracked_ptr` points to, a class, an abstract base, `int`. It is neither hashed nor compared: the key is the object's identity. |
| `T` | The type of the values: any object type constructible from the arguments of the insertion; it need not be copyable or movable, as `emplace` constructs it in the entry. It may be, or hold, tracked pointers. |

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
| [(constructor)](weak_multimap.md) | constructs an empty map, or takes one over |
| `(destructor)` | destroys the values; the nodes are left to the collector |
| [operator=](operator_assign.md) | takes the entries of another map over |

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
| [insert](insert.md) | inserts one more value for an object |
| [emplace](emplace.md) | constructs one more value for an object in place |
| [erase](erase.md) | erases the entries of an object, or the one an iterator stands on |
| [sweep](sweep.md) | erases the entries whose objects are gone |

#### Lookup

| Function | Description |
|---|---|
| [count](count.md) | the number of entries of an object |
| [find](find.md) | the first entry of an object |
| [contains](contains.md) | checks whether an object has an entry |
| [equal_range](equal_range.md) | the range of the entries of an object |

## Complexity

- `find`, `contains`: constant on average. `count`, `equal_range`, `erase` by the object: constant on average, plus
  linear in the number of the object's entries.
- `insert`, `emplace`: constant on average, plus, every so many insertions, a sweep linear in the number of
  entries: amortized constant, as the sweep comes after as many insertions as the map had entries after the last
  one, 16 at least.
- `sweep`, `clear`: linear in the number of entries. `size`, `empty`, `end`: constant. `begin`: constant, plus the
  dead entries before the first live one, which it passes.

## Iterator invalidation

| Operations | Invalidated |
|---|---|
| all read-only operations | never |
| `insert`, `emplace`, `sweep` | never: an iterator on a live entry holds its object, so no sweep drops its entry, a growth of the table keeps every node where it is, and a walk of an `equal_range` ends with the object's entries, whatever becomes of the entry its end stands on |
| `erase` | the iterators to the erased entries |
| `clear`, `operator=` | always |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Node {
    int value;
};

void annotate_a_temporary(weak_multimap<Node, string>& meta) {
    tracked_ptr node = make_tracked<Node>(42);
    meta.insert(node, "created by the parser");
    meta.insert(node, "checked");
    for (auto [first, last] = meta.equal_range(node); first != last; ++first) {
        println("{}: {}", first->key->value, first->value);
    }
}

int main() {
    // Metadata attached to any object, as many strings as needed: the map
    // holds its objects weakly, and the entries die with the object.
    weak_multimap<Node, string> meta;  // on the stack, as any tracked pointer
    annotate_a_temporary(meta);  // the node's last strong pointer is gone with the frame

    collector::clear_stack();  // the dead frame zeroed: nothing on the stack keeps the node
    collector::force_collect(true);  // optional, for the demonstration: the cycle clears the key
    size_t entries = meta.size();
    size_t swept = meta.sweep();
    println("{} entries, {} swept, {} left", entries, swept, meta.size());
}
```

Output:

```text
42: checked
42: created by the parser
2 entries, 2 swept, 0 left
```

## See also

- [weak_map](../weak_map/README.md): one value per object
- [weak_set](../weak_set/README.md): the objects alone; [weak_ptr](../weak_ptr/README.md): the key
- [expiry_queue](../expiry_queue/README.md): a function called with the object when it is found unreachable, for the cleanup
  that needs the object
- [multimap](../multimap/README.md): the table underneath
- README: [Weak containers](../README.md#weak-containers) under [Weak pointers](../weak_ptr/README.md),
  [The rules](../README.md#the-rules)
- `tests/containers/weak_map.cpp`: every behaviour above, checked.
