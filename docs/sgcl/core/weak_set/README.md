[sgcl](../../README.md) › [core](../README.md)

# sgcl::weak_set\<Key\>

```cpp
#include "sgcl/core/weak_set.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class Key>
    class weak_set;
}
```

`sgcl::weak_set<Key>` is a set of objects that does not keep them alive: the [weak_map](../weak_map/README.md) of nothing but
keys. An object is inserted, found and erased by a `tracked_ptr<Key>` to it and held by a [weak_ptr](../weak_ptr/README.md);
an entry whose object the collector has found unreachable is dead: never found, passed over by the iteration,
dropped by a sweep. Objects registered somewhere without being owned there: the listeners, the open windows, the
instances of a class, a set that forgets. [concurrent::weak_set](../../concurrent/weak_set/README.md) is the set shared by
threads without a lock.

Hashing, equality, the sweeps and the rules are those of `weak_map`: the entries are hashed and compared by the
object's address, which the collector clears from the weak pointer's cell before the address can be handed out
again, so a dead entry equals nothing and never blocks the entry of the object that takes the slot next; dead
entries are swept out every so many insertions, as many as the set has entries, and on [sweep](sweep.md);
[size](size.md) counts the entries a sweep has not yet dropped.

What differs from `std::unordered_set`: the element is the object, compared by identity, not by value, and held
only while an iterator stands on it; the set is moved but not copied. What differs from Java, which has no weak set
of its own and makes one with `Collections.newSetFromMap(new WeakHashMap<>())`: the set compares by identity, not by
`equals`. Go's library has no weak set.

## Rules

- A `weak_set` holds tracked pointers, so it lives on a stack or inside a managed object
  ([The rules](../README.md#the-rules), 1).
- An entry is dead once a cycle has found its object unreachable; between the object becoming unreachable and that
  cycle, it is found and visited like any other: the lag of any garbage collector.
- The iteration hands out the object as a strong pointer, held while the iterator stands on the entry: the object
  cannot die under it. An iterator is a tracked object then, and lives where the set's pointers may.
- A null pointer is not an object: [insert](insert.md) asserts in debug builds, the lookups find nothing.
- Thread safety is that of `std::unordered_set`: concurrent readers, or one writer, with the program's own
  synchronization ([The rules](../README.md#the-rules), 6). The collector clearing an object's cell at the same time
  is safe.

## Template parameters

| Parameter | Description |
|---|---|
| `Key` | The type of the objects: any type a `tracked_ptr` points to, a class, an abstract base, `int`. It is neither hashed nor compared: the element is the object's identity. |

## Member types

| Type | Definition |
|---|---|
| `key_type` | `Key` |
| `key_pointer` | `tracked_ptr<Key>` |
| `weak_type` | `weak_ptr<Key>` |
| `size_type` | `size_t` |
| `reference` | `key_pointer`: what an iterator gives out, the object, held |
| `iterator` | a forward iterator over the live objects, of a class of the library; `*it` is a `key_pointer`, and `it->` is the pointer's own `->`, so `(*it)->member` |
| `const_iterator` | `iterator`: the set gives nothing to write through an iterator |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](weak_set.md) | constructs an empty set, or takes one over |
| `(destructor)` | erases the entries; the nodes are left to the collector |
| [operator=](operator_assign.md) | takes the entries of another set over |

#### Iterators

| Function | Description |
|---|---|
| [begin, cbegin](begin.md) | an iterator to the first live object |
| [end, cend](end.md) | an iterator past the last entry |

#### Capacity

| Function | Description |
|---|---|
| [empty](empty.md) | checks whether the set holds an entry, a dead one included |
| [size](size.md) | the number of entries, the dead ones not yet swept included |

#### Modifiers

| Function | Description |
|---|---|
| [clear](clear.md) | erases every entry |
| [insert](insert.md) | inserts an object, unless the set holds it |
| [erase](erase.md) | erases an object, or the entry an iterator stands on |
| [sweep](sweep.md) | erases the entries whose objects are gone |

#### Lookup

| Function | Description |
|---|---|
| [count](count.md) | the number of entries of an object, 0 or 1 |
| [find](find.md) | the entry of an object |
| [contains](contains.md) | checks whether the set holds an object |

## Complexity

- `find`, `contains`, `count`, `erase`: constant on average.
- `insert`: constant on average, plus, every so many insertions, a sweep linear in the number of entries: amortized
  constant, as the sweep comes after as many insertions as the set had entries after the last one, 16 at least.
- `sweep`, `clear`: linear in the number of entries. `size`, `empty`, `end`: constant. `begin`: constant, plus the
  dead entries before the first live one, which it passes.

## Iterator invalidation

| Operations | Invalidated |
|---|---|
| all read-only operations, `insert`, `sweep` | never: an iterator stands on a live entry and holds its object, so no sweep drops its entry, and a growth of the table keeps every node where it is |
| `erase` | the iterators to the erased entry |
| `clear`, `operator=` | always |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Window {
    explicit Window(int id) : id(id) {}
    int id;
};

void open_a_dialog(weak_set<Window>& windows) {
    tracked_ptr dialog = make_tracked<Window>(2);
    windows.insert(dialog);
    println("{} windows", windows.size());
}

int main() {
    // Every window there is, without owning any: a window is gone when
    // its owner drops it, and the set notices
    weak_set<Window> windows;
    tracked_ptr main_window = make_tracked<Window>(1);
    windows.insert(main_window);
    open_a_dialog(windows);  // the dialog's last strong pointer is gone with the frame

    collector::clear_stack();  // the dead frame zeroed: nothing on the stack keeps the dialog
    collector::force_collect(true);  // optional, for the demonstration: the cycle clears the entry
    for (auto window : windows) {  // tracked_ptr<Window>, held: the live ones
        println("window {}", window->id);
    }
    size_t gone = windows.sweep();
    println("{} gone, {} left", gone, windows.size());
}
```

Output:

```text
2 windows
window 1
1 gone, 1 left
```

## See also

- [weak_map](../weak_map/README.md): a value for each object; [weak_multimap](../weak_multimap/README.md): several
- [weak_ptr](../weak_ptr/README.md): what holds the object
- [set](../set/README.md): the table underneath
- [concurrent::weak_set](../../concurrent/weak_set/README.md): the set shared by threads without a lock
- README: [Weak containers](../README.md#weak-containers) under [Weak pointers](../weak_ptr/README.md)
- `tests/containers/weak_map.cpp`: the set's behaviour, checked with the maps'.
