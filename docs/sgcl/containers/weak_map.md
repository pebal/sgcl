# sgcl::weak_map, sgcl::weak_multimap

```cpp
#include "sgcl/containers/weak_map.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class Key, class T>
    class weak_map;
    template<class Key, class T>
    class weak_multimap;
}
```

`weak_map<Key, T>` maps objects to values without keeping the objects alive. The key is the object itself, its identity and not its contents: an entry is looked up, made and erased by a `tracked_ptr<Key>` to the object, and held by a [`weak_ptr`](../core/weak_ptr.md). An entry whose object the collector has found unreachable is dead: never found, passed over by the iteration, dropped by a sweep. Metadata attached to objects from outside, a cache keyed by the object, a registry that forgets. `weak_multimap` holds several values per object; [`weak_set`](weak_set.md) holds the objects alone.

The entries are hashed and compared by the object's address, which the weak pointer's cell holds while the object lives and the collector clears before the address can be handed out again (the weak phase runs before the sweep that frees the slot: [Weak pointers](../core/README.md#weak-pointers)). So a dead entry equals nothing, its own key included, and can neither be found nor block the entry of the object that takes the slot next. Dead entries are swept out every so many insertions, as many as the map has entries, so that a pass costs less than the insertions that paid for it, and on `sweep()`; `size()` counts the entries a sweep has not yet dropped. The values are the map's own, destroyed with the entry. A value holding a strong pointer to its own key keeps the key alive, and the entry with it: the map has no ephemerons.

## Rules

- A `weak_map` holds tracked pointers, so it lives on a stack or inside a managed object ([The rules](../core/README.md#the-rules), 1).
- The values may be, or hold, tracked pointers: the nodes are managed objects. A value that reaches its own key keeps the key, and so the entry, alive for as long as the entry is in the map.
- An entry is dead once a cycle has found its object unreachable; between the object becoming unreachable and that cycle, it is found and visited like any other: the lag of any garbage collector.
- The iteration hands out the object as a strong pointer, held while the iterator stands on the entry: the entry cannot die under it. An iterator is a tracked object then, and lives where the map's pointers may.
- The value of an entry is stable while the entry is in the map; a reference to it is invalid once the entry is erased or swept, as in `std::unordered_map`.
- Thread safety is that of `std::unordered_map`: concurrent readers, or one writer, with the program's own synchronization ([The rules](../core/README.md#the-rules), 6). The collector clearing a key's cell at the same time is safe.

## Members

### Types

```cpp
using key_type = Key;
using key_pointer = tracked_ptr<Key>;               // tracked_ptr<Key>, or sgcl::tracked_ptr<Key>
using mapped_type = T;
using weak_type = weak_ptr<Key>;
using size_type = size_t;
struct reference { key_pointer key; T& value; };   // what an iterator gives out
using iterator = /* forward iterator over the live entries */;
```

`reference` is what `*it` returns: the object, held, and the value; `it->key`, `it->value`. There is no `const_iterator`, and `begin()`/`end()` are not `const`: standing on an entry holds its object, which is a write to the iterator, not to the map.

### Constructors

```cpp
weak_map();
weak_map(weak_map&&) noexcept;
weak_map& operator=(weak_map&&) noexcept;
weak_map(const weak_map&) = delete;           // the values would be copied, the keys shared: say which
```

### begin, end

```cpp
iterator begin() noexcept;
iterator end() noexcept;
```

The live entries, in no particular order, each once; the dead ones are passed over without being dropped.

```cpp
struct Node { int value; };
sgcl::weak_map<Node, sgcl::string> names;
sgcl::tracked_ptr node = sgcl::make_tracked<Node>(1);
names[node] = "one";
for (auto [key, value] : names) {   // key: sgcl::tracked_ptr<Node>, value: sgcl::string&
    std::cout << key->value << ' ' << value << '\n';
}
```

### find, count, contains

```cpp
iterator find(const key_pointer& object);
size_type count(const key_pointer& object) const;
bool contains(const key_pointer& object) const;
```

The entry of the object, or `end()`; a null pointer has none, and an object that is gone has none.

### operator[]

```cpp
T& operator[](const key_pointer& object);
```

The value of the object, a `T()` made in the entry if the object has none: one search, the entry built in place from the pointer when it finds nothing. A null pointer is not an object (debug builds assert).

```cpp
sgcl::weak_map<Node, int> visits;
visits[node] += 1;
```

### emplace, insert, insert_or_assign

```cpp
template<class... A> std::pair<iterator, bool> emplace(const key_pointer& object, A&&... a);
std::pair<iterator, bool> insert(const key_pointer& object, const T& value);
std::pair<iterator, bool> insert(const key_pointer& object, T&& value);
template<class V> std::pair<iterator, bool> insert_or_assign(const key_pointer& object, V&& value);
```

A value for the object, `T(a...)` for `emplace`, unless the object has one; whether one was added. One search: the value is built in place, in the entry, only when the search finds none, so a hit costs a lookup and builds nothing. `insert_or_assign` replaces the value the object has. Every insertion counts towards the next sweep.

```cpp
auto [it, added] = names.emplace(node, "one");   // added: false, the entry of "one"
names.insert_or_assign(node, "uno");
assert(names[node] == "uno");
```

### erase

```cpp
size_type erase(const key_pointer& object);
iterator erase(iterator pos);
```

The entry of the object, dropped: how many (0 or 1). By iterator: the next live entry up to the iterator's own bound, `end()` or, in a `weak_multimap`, the end of the `equal_range` it came from, so a walk that erases a range stops where the range does, a dead entry at the bound included.

### sweep, clear

```cpp
size_type sweep();
void clear() noexcept;
```

`sweep()` drops the entries whose objects are gone and returns how many; the map does it by itself every so many insertions, and a program that inserts little and wants the memory back calls it. `clear()` drops every entry.

### size, empty

```cpp
size_type size() const noexcept;
bool empty() const noexcept;
```

The entries, the dead ones not yet swept included; exact right after a `sweep()`.

## weak_multimap

The same class with several values per object: `emplace(object, a...)`, `insert(object, value)` add one more value and return its iterator; `find` is the first entry of the object; `equal_range(object)` is the range of its entries; `erase(object)` drops them all and `count(object)` counts them; no `operator[]`, no `insert_or_assign`.

```cpp
std::pair<iterator, iterator> equal_range(const key_pointer& object);
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

struct Node {
    int value;
};

int main() {
    // Metadata attached to any object, as many strings as needed: the map
    // holds its objects weakly, and the entries die with the object.
    sgcl::weak_multimap<Node, sgcl::string> meta;   // on the stack, as any tracked pointer
    {
        sgcl::tracked_ptr node = sgcl::make_tracked<Node>(42);
        meta.insert(node, "created by the parser");
        meta.insert(node, "checked");
        for (auto [first, last] = meta.equal_range(node); first != last; ++first) {
            std::cout << first->key->value << ": " << first->value << "\n";
        }
    }   // the last strong pointer is gone
    sgcl::collector::clear_stack();          // the dead frame zeroed, so that the conservative scan keeps nothing
    sgcl::collector::force_collect(true);    // optional, for the demonstration: the cycle clears the key
    std::cout << meta.size() << " entries, " << meta.sweep() << " swept, " << meta.size() << " left\n";
}
```

The output:

```
42: checked
42: created by the parser
2 entries, 2 swept, 0 left
```

Output: `42: checked` and `42: created by the parser` in either order, then `2 entries, 2 swept, 0 left`.

## See also

- [weak_set](weak_set.md): the objects alone; [weak_ptr](../core/weak_ptr.md): the key
- [expiry_queue](expiry_queue.md): a function called with the object when it is found unreachable, for the cleanup that needs the object
- [map](map.md): the table underneath
- README: [Weak containers](README.md#weak-containers) under [Weak pointers](../core/README.md#weak-pointers), [The rules](../core/README.md#the-rules)
- `tests/containers/weak_map.cpp`: every behaviour above, checked.
