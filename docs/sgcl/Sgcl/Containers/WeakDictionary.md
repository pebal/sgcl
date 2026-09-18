# Sgcl::WeakDictionary, Sgcl::WeakMultiDictionary

```cpp
#include "sgcl/Sgcl/Containers/WeakDictionary.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class Key, class Value>
    class WeakDictionary;
    template<class Key, class Value>
    class WeakMultiDictionary;
}
```

The same classes in the `sgcl` interface: [weak_map, weak_multimap](../../containers/weak_map.md).

`WeakDictionary<Key, Value>` maps objects to values without keeping the objects alive. The key is the object itself, its identity and not its contents: an entry is looked up, made and removed by a `Ptr<Key>` to the object, and held by a [`WeakPtr`](../Core/WeakPtr.md). An entry whose object the collector has found unreachable is dead: never found, passed over by the iteration, dropped by a sweep. Metadata attached to objects from outside, a cache keyed by the object, a registry that forgets. `WeakMultiDictionary` holds several values per object; [`WeakHashSet`](WeakHashSet.md) holds the objects alone.

The entries are hashed and compared by the object's address, which the weak pointer's cell holds while the object lives and the collector clears before the address can be handed out again (the weak phase runs before the sweep that frees the slot: [Weak pointers](../../core/README.md#weak-pointers)). So a dead entry equals nothing, its own key included, and can neither be found nor block the entry of the object that takes the slot next. Dead entries are swept out every so many insertions, as many as the dictionary has entries, so that a pass costs less than the insertions that paid for it, and on `Sweep()`; `Count()` counts the entries a sweep has not yet dropped. The values are the dictionary's own, destroyed with the entry. A value holding a strong pointer to its own key keeps the key alive, and the entry with it: the dictionary has no ephemerons.

## Rules

- A `WeakDictionary` holds tracked pointers, so it lives on a stack or inside a managed object ([The rules](../../core/README.md#the-rules), 1).
- The values may be, or hold, tracked pointers: the nodes are managed objects. A value that reaches its own key keeps the key, and so the entry, alive for as long as the entry is in the dictionary.
- An entry is dead once a cycle has found its object unreachable; between the object becoming unreachable and that cycle, it is found and visited like any other: the lag of any garbage collector.
- The iteration hands out the object as a strong pointer, held while the iterator stands on the entry: the entry cannot die under it. An iterator is a tracked object then, and lives where the dictionary's pointers may.
- The value of an entry is stable while the entry is in the dictionary; a reference to it (what `Find` returns) is invalid once the entry is removed or swept, as in `std::unordered_map`.
- Thread safety is that of `std::unordered_map`: concurrent readers, or one writer, with the program's own synchronization ([The rules](../../core/README.md#the-rules), 6). The collector clearing a key's cell at the same time is safe.

## Members

### Types

```cpp
using KeyType = Ptr<Key>;
using ValueType = Value;
using InnerType = sgcl::weak_map<Key, Value>;
using SizeType = size_t;
using Iterator = InnerType::iterator;               // forward, over the live entries
```

`*it` is `{key, value}`: the object, held, and the value; `it->key`, `it->value`. There is no const iterator, and the free `begin`/`end` are not `const`: standing on an entry holds its object, which is a write to the iterator, not to the dictionary.

### Constructors

```cpp
WeakDictionary();
WeakDictionary(WeakDictionary&&) noexcept;
WeakDictionary& operator=(WeakDictionary&&) noexcept;
WeakDictionary(const WeakDictionary&) = delete;      // the values would be copied, the keys shared: say which
```

### begin, end

```cpp
Iterator begin(WeakDictionary&) noexcept;            // free functions
Iterator end(WeakDictionary&) noexcept;
```

The live entries, in no particular order, each once; the dead ones are passed over without being dropped.

```cpp
struct Node { int value; };
WeakDictionary<Node, String> names;
Ptr node = Make<Node>(1);
names[node] = "one";
for (auto [key, value] : names) {   // key: Ptr<Node>, value: String&
    std::cout << key->value << ' ' << value << '\n';
}
```

### Find, FindEntry, ContainsKey

```cpp
Value* Find(const Ptr<Key>& object);
Iterator FindEntry(const Ptr<Key>& object);
bool ContainsKey(const Ptr<Key>& object) const;
```

The value of the object as a pointer, null when the object has none; the entry as an iterator, `end(d)` when absent. A null pointer has no entry, and an object that is gone has none.

### operator[]

```cpp
Value& operator[](const Ptr<Key>& object);
```

The value of the object, a `Value()` made and inserted if the object has none. A null pointer is not an object (debug builds assert).

```cpp
struct Node { int value; };
WeakDictionary<Node, int> visits;
Ptr node = Make<Node>(1);
visits[node] += 1;
```

### Add, Emplace, Set

```cpp
bool Add(const Ptr<Key>& object, const Value& value);
bool Add(const Ptr<Key>& object, Value&& value);
template<class... A> bool Emplace(const Ptr<Key>& object, A&&... a);
void Set(const Ptr<Key>& object, const Value& value);
void Set(const Ptr<Key>& object, Value&& value);
```

A value for the object, `Value(a...)` for `Emplace`, unless the object has one; whether one was added. `Set` replaces the value the object has. Every insertion counts towards the next sweep.

```cpp
struct Node { int value; };
WeakDictionary<Node, String> names;
Ptr node = Make<Node>(1);
names[node] = "one";
bool added = names.Emplace(node, "one");         // false: the entry of "one" stays
names.Set(node, "uno");
assert(names[node] == "uno");
```

### Remove, RemoveAt

```cpp
bool Remove(const Ptr<Key>& object);
Iterator RemoveAt(Iterator pos);
```

The entry of the object, dropped: whether there was one. By iterator: the next live entry.

### Sweep, Clear

```cpp
SizeType Sweep();
void Clear() noexcept;
```

`Sweep()` drops the entries whose objects are gone and returns how many; the dictionary does it by itself every so many insertions, and a program that inserts little and wants the memory back calls it. `Clear()` drops every entry.

### Count, IsEmpty

```cpp
SizeType Count() const noexcept;
bool IsEmpty() const noexcept;
```

The entries, the dead ones not yet swept included; exact right after a `Sweep()`.

### Inner

```cpp
InnerType& Inner() noexcept;
const InnerType& Inner() const noexcept;
```

The table inside, as its own type.

## WeakMultiDictionary

The same class with several values per object: `Emplace(object, a...)`, `Add(object, value)` add one more value; `Find` is the first value of the object, `FindEntry` its first entry; `Values(object)` is the [Range](../Core/Range.md) of its entries; `Remove(object)` drops them all and `CountOf(object)` counts them; no `operator[]`, no `Set`.

```cpp
Range<Iterator> Values(const Ptr<Key>& object);
SizeType CountOf(const Ptr<Key>& object) const;
```

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>

struct Node {
    int value;
};

int main() {
    // Metadata attached to any object, as many strings as needed: the
    // dictionary holds its objects weakly, and the entries die with the object.
    WeakMultiDictionary<Node, String> meta;   // on the stack, as any tracked pointer
    {
        Ptr node = Make<Node>(42);
        meta.Add(node, "created by the parser");
        meta.Add(node, "checked");
        for (auto [key, value] : meta.Values(node)) {
            std::cout << key->value << ": " << value << "\n";
        }
    }   // the last strong pointer is gone
    Collector::ClearStack();           // the dead frame zeroed, so that the conservative scan keeps nothing
    Collector::Collect(true);          // optional, for the demonstration: the cycle clears the key
    std::cout << meta.Count() << " entries, " << meta.Sweep() << " swept, " << meta.Count() << " left\n";
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

- [WeakHashSet](WeakHashSet.md): the objects alone; [WeakPtr](../Core/WeakPtr.md): the key
- [ExpiryQueue](ExpiryQueue.md): a function called with the object when it is found unreachable, for the cleanup that needs the object
- [Dictionary](Dictionary.md): the table underneath
- README: [Weak containers](../../containers/README.md#weak-containers) under [Weak pointers](../../core/README.md#weak-pointers), [The rules](../../core/README.md#the-rules)
