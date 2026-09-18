# Sgcl::WeakHashSet

```cpp
#include "sgcl/Sgcl/Containers/WeakDictionary.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class Key>
    class WeakHashSet;
}
```

The same class in the `sgcl` interface: [weak_set](../../containers/weak_set.md).

`WeakHashSet<Key>` is a set of objects that does not keep them alive: the [`WeakDictionary`](WeakDictionary.md) of nothing but keys. An object is added, found and removed by a `Ptr<Key>` to it and held by a [`WeakPtr`](../Core/WeakPtr.md); an entry whose object the collector has found unreachable is dead: never found, passed over by the iteration, dropped by a sweep. Objects registered somewhere without being owned there: the listeners, the open windows, the instances of a class, a set that forgets. Hashing, equality, the sweeps and the rules are those of `WeakDictionary`.

## Members

```cpp
using ValueType = Ptr<Key>;
using InnerType = sgcl::weak_set<Key>;
using SizeType = size_t;
using Iterator = InnerType::iterator;               // forward, over the live objects

WeakHashSet();
WeakHashSet(WeakHashSet&&) noexcept;
WeakHashSet& operator=(WeakHashSet&&) noexcept;

Iterator begin(WeakHashSet&) noexcept;              // free functions: the live objects, each once
Iterator end(WeakHashSet&) noexcept;
Iterator FindEntry(const Ptr<Key>& object);
bool Contains(const Ptr<Key>& object) const;
bool Add(const Ptr<Key>& object);                   // whether it was added
bool Remove(const Ptr<Key>& object);
Iterator RemoveAt(Iterator pos);
SizeType Sweep();                                   // drops the dead entries: how many
void Clear() noexcept;
SizeType Count() const noexcept;                    // the dead ones not yet swept included
bool IsEmpty() const noexcept;
InnerType& Inner() noexcept;
```

`*it` is the object as a `Ptr<Key>`, held while the iterator stands on it; `it->` is the pointer's own `->`, so `(*it)->member`. A null pointer is not an object: `Add` asserts in debug builds, the lookups find nothing.

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>

struct Window {
    explicit Window(int id) : id(id) {}
    int id;
};

int main() {
    // Every window there is, without owning any: a window is gone when
    // its owner drops it, and the set notices
    WeakHashSet<Window> windows;
    Ptr mainWindow = Make<Window>(1);
    windows.Add(mainWindow);
    {
        Ptr dialog = Make<Window>(2);
        windows.Add(dialog);
        std::cout << windows.Count() << " windows\n";              // 2
    }   // the dialog's last strong pointer is gone
    Collector::ClearStack();           // the dead frame zeroed, so that the conservative scan keeps nothing
    Collector::Collect(true);          // optional, for the demonstration: the cycle clears the entry
    for (auto window : windows) {               // Ptr<Window>, held: the live ones
        std::cout << "window " << window->id << "\n";              // window 1
    }
    std::cout << windows.Sweep() << " gone, " << windows.Count() << " left\n";   // 1 gone, 1 left
}
```

The output:

```
2 windows
window 1
1 gone, 1 left
```

## See also

- [WeakDictionary](WeakDictionary.md), [WeakPtr](../Core/WeakPtr.md), [HashSet](HashSet.md)
- README: [Weak containers](../../containers/README.md#weak-containers) under [Weak pointers](../../core/README.md#weak-pointers)
