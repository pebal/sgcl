# Sgcl::Any

```cpp
#include "sgcl/Sgcl/Core/Any.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    class Any;
}
```

The same class in the `sgcl` interface: [any](../../core/any.md).

`Any` is `std::any` for values that hold tracked pointers. `std::any` keeps a small value in a buffer inside itself, where a `Ptr` would share its word with the data of other values (the offset leaves the pointer map by elimination: [README: Pointer maps](../../../garbage_collector/overview.md#pointer-maps)), and a large one on the unmanaged heap, where a `Ptr` may not live. Here a value goes to one of three places by what it is: a pointer word (`Ptr`, [`WeakPtr`](WeakPtr.md)) into a word of the `Any` that holds null or an address and nothing else; a small value that cannot hold a pointer (16 bytes at most, trivially default constructible or smaller than a word) into a buffer inside the `Any`; anything else (a struct with a `Ptr` member, a container, a `std::string`: whatever the collector cannot rule out) into a managed node of its own, held by a pointer in the same word and traced through its own pointer map, so that a value pointing back at the `Any`'s owner is a cycle collected like any other. The value is destroyed the moment the `Any` drops it, on that thread, as a container destroys a removed element; the node is reclaimed by the collector later. An `Any` is 32 bytes, as `std::any` in libc++.

The interface is that of `std::any` under the interface's names: the constructors, `in_place_type`, `Emplace`, `Reset`, `Swap`, `HasValue`, `Type`, `Is<T>`, `Get<T>` (a pointer, null on another type), `As<T>` (the value, `bad_any_cast` on another type, the one of `std`), `MakeAny`. What differs: a copy of a value in a node is a node of its own; a moved-from `Any` is empty; a value larger than a page is not supported.

The `Any`'s word is a `Ptr`, so an `Any` lives where one may, as the containers do: on a stack or inside a managed object. What the `Any` holds follows the rules of its type where the `Any` lives, as a member would; a value in a node is anywhere's.

## Rules

- An `Any` lives where a `Ptr` may ([The rules](../../core/README.md#the-rules), 1); what it holds follows the rules of its type where the `Any` lives. A value in a node is fine anywhere.
- A pointer in the word is destroyed by `Reset`, an assignment or the destructor: its object is unreferenced from then on. A value in a node is destroyed at the same moment, its destructor on the calling thread; the node goes back with the next sweep.
- A value in a node is traced: one that points back at the object holding the `Any` is a cycle, collected when nothing else reaches it.
- `Get<T>()` and `As<T&>()` on a value in a node are a pointer or a reference into that node: valid while the `Any` holds the value.
- Thread safety is that of `std::any` ([The rules](../../core/README.md#the-rules), 6).

## Members

```cpp
using InnerType = sgcl::any;

Any() noexcept;
Any(const Any&);
Any(Any&&) noexcept;
template<class T> Any(T&& value);                                   // decay_t<T> copy constructible, not an Any
template<class T, class... A> explicit Any(std::in_place_type_t<T>, A&&...);
template<class T, class U, class... A> explicit Any(std::in_place_type_t<T>, std::initializer_list<U>, A&&...);
explicit Any(InnerType a) noexcept;
~Any();

Any& operator=(const Any&);
Any& operator=(Any&&) noexcept;
template<class T> Any& operator=(T&& value);

template<class T, class... A> std::decay_t<T>& Emplace(A&&...);
template<class T, class U, class... A> std::decay_t<T>& Emplace(std::initializer_list<U>, A&&...);
void Reset() noexcept;
void Swap(Any&) noexcept;
bool HasValue() const noexcept;
const std::type_info& Type() const noexcept;          // typeid(void) when empty
template<class T> bool Is() const noexcept;           // Type() == typeid(T)
template<class T> T* Get() noexcept;                  // the value, null on another type
template<class T> const T* Get() const noexcept;
template<class T> T As() &;                           // T, T&: bad_any_cast on another type
template<class T> T As() const&;                      // T, const T&
template<class T> T As() &&;                          // T, T&&
InnerType& Inner() noexcept;
const InnerType& Inner() const noexcept;
```

The free functions, in `Sgcl`:

```cpp
void swap(Any&, Any&) noexcept;
template<class T, class... A> Any MakeAny(A&&...);
template<class T, class U, class... A> Any MakeAny(std::initializer_list<U>, A&&...);
```

`Get<T>` and `As<T>` compare `typeid(T)` with the type held, as `std::any_cast` does.

```cpp
struct Node { int value; };
struct Counted { Ptr<Node> node; int count; };

Any a = Ptr(Make<Node>(1));           // in the word
Any b = Counted{Make<Node>(2), 7};                // in a managed node of its own
Any c = 3;                                              // in the buffer
assert(a.As<Ptr<Node>>()->value == 1);
assert(b.As<Counted&>().count == 7);
assert(*c.Get<int>() == 3 && c.Get<double>() == nullptr);
b.Reset();                                                       // the Counted destroyed now; its Node dies with the next cycle, the node with it
```

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>

// Properties attached to objects by name, of any type, some of them
// tracked pointers to other objects: a SortedDictionary of Any inside
// the managed object, its pointers followed. A property pointing back
// at its own object is a cycle: collected.
struct Node {
    int id;
    SortedDictionary<String, Any> properties;
};

int main() {
    Ptr a = Make<Node>(1);
    Ptr b = Make<Node>(2);
    a->properties["weight"] = 2.5;                              // in the buffer
    a->properties["label"] = String("first");                   // a string, an object with a pointer word inside: in a managed node of its own
    a->properties["peer"] = b;                                  // a Ptr in the word: b lives while a does
    a->properties["peers"] = List<Ptr<Node>>{b, a};   // a container, in a managed node; a inside: a cycle through a, collected with a
    b = nullptr;
    Collector::Collect(true);                          // optional, for the demonstration only
    auto& peer = a->properties["peer"].As<Ptr<Node>&>();
    std::cout << peer->id << " " << a->properties["weight"].As<double>() << "\n";   // 2 2.5
    return 0;
}
```

The output:

```
2 2.5
```

## See also

- [Variant](Variant.md): the same for a closed set of alternatives; [Ptr](Ptr.md), [WeakPtr](WeakPtr.md), [UniquePtr](UniquePtr.md)
- README: [variant, any, function and expected](../../core/README.md#variant-any-function-and-expected), [Pointer maps](../../../garbage_collector/overview.md#pointer-maps), [The rules](../../core/README.md#the-rules)
