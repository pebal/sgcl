# sgcl::weak_set

```cpp
#include "sgcl/weak_set.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class Key, template<class> class Ptr = tracked_ptr>
    class weak_set;
}
```

`weak_set<Key>` is a set of objects that does not keep them alive: the [`weak_map`](weak_map.md) of nothing but keys. An object is inserted, found and erased by a `tracked_ptr<Key>` to it and held by a [`weak_ptr`](weak_ptr.md); an entry whose object the collector has found unreachable is dead: never found, passed over by the iteration, dropped by a sweep. Objects registered somewhere without being owned there: the listeners, the open windows, the instances of a class, a set that forgets. Hashing, equality, the sweeps, `Ptr` and the rules are those of `weak_map`; `gc::weak_set` ([gc/gc.h](README.md#the-gc-namespace)) lives anywhere.

## Members

```cpp
using key_type = Key;
using key_pointer = Ptr<Key>;
using weak_type = weak_ptr<Key, Ptr>;
using size_type = size_t;
using reference = key_pointer;               // what an iterator gives out: the object, held
using iterator = /* forward iterator over the live objects */;

weak_set();
weak_set(weak_set&&) noexcept;
weak_set& operator=(weak_set&&) noexcept;

iterator begin() noexcept;                   // the live objects, each once
iterator end() noexcept;
iterator find(const key_pointer& object);
size_type count(const key_pointer& object) const;
bool contains(const key_pointer& object) const;
std::pair<iterator, bool> insert(const key_pointer& object);   // whether it was added
size_type erase(const key_pointer& object);
iterator erase(iterator pos);
size_type sweep();                           // drops the dead entries: how many
void clear() noexcept;
size_type size() const noexcept;             // the dead ones not yet swept included
bool empty() const noexcept;
```

`*it` is the object as a `key_pointer`, held while the iterator stands on it; `it->` is the pointer's own `->`, so `(*it)->member`. A null pointer is not an object: `insert` asserts in debug builds, the lookups find nothing.

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

struct Window {
    explicit Window(int id) : id(id) {}
    int id;
};

int main() {
    // Every window there is, without owning any: a window is gone when
    // its owner drops it, and the set notices
    gc::weak_set<Window> windows;
    gc::tracked_ptr main_window = gc::make_tracked<Window>(1);
    windows.insert(main_window);
    {
        gc::tracked_ptr dialog = gc::make_tracked<Window>(2);
        windows.insert(dialog);
        std::cout << windows.size() << " windows\n";              // 2
    }   // the dialog's last strong pointer is gone
    gc::collector::clear_stack();          // the dead frame zeroed, so that the conservative scan keeps nothing
    gc::collector::force_collect(true);    // optional, for the demonstration: the cycle clears the entry
    for (auto window : windows) {          // tracked_ptr<Window>, held: the live ones
        std::cout << "window " << window->id << "\n";              // window 1
    }
    std::cout << windows.sweep() << " gone, " << windows.size() << " left\n";   // 1 gone, 1 left
}
```

## See also

- [weak_map](weak_map.md), [weak_ptr](weak_ptr.md), [unordered_set](unordered_set.md)
- README: [Weak containers](../README.md#weak-containers) under [Weak pointers](../README.md#weak-pointers)
- `tests/weak_map.cpp`: the set's behaviour, checked with the maps'.
