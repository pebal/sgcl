# sgcl::any

```cpp
#include "sgcl/any.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<template<class> class Ptr>
    class basic_any;
    using any = basic_any<tracked_ptr>;
}
```

`sgcl::any` is `std::any` for values that hold tracked pointers. `std::any` keeps a small value in a buffer inside itself, where a `tracked_ptr` would share its word with the data of other values (the offset leaves the pointer map by elimination: [README: Pointer maps](../README.md#pointer-maps)), and a large one on the unmanaged heap, where a `tracked_ptr` may not live. Here a value goes to one of three places by what it is: a pointer word (`tracked_ptr` of either kind, [`weak_ptr`](weak_ptr.md)) into a word of the `any` that holds null or an address and nothing else; a small value that cannot hold a pointer (16 bytes at most, trivially default constructible or smaller than a word) into a buffer inside the `any`; anything else (a struct with a `tracked_ptr` member, a container, a `std::string`: whatever the collector cannot rule out) into a managed node of its own, held by a pointer in the same word and traced through its own pointer map, so that a value pointing back at the `any`'s owner is a cycle collected like any other. The value is destroyed the moment the `any` drops it, on that thread, as a container destroys an erased element; the node is reclaimed by the collector later. An `any` is 32 bytes, as `std::any` in libc++.

The interface is that of `std::any`: the constructors, `in_place_type`, `emplace`, `reset`, `swap`, `has_value`, `type`, `any_cast` in every form, `make_any`, `bad_any_cast` (the one of `std`). What differs: a copy of a value in a node is a node of its own; a moved-from `any` is empty; a value larger than a page is not supported.

`Ptr` is the kind of the `any`'s word, and so where the `any` lives, as for the containers: `sgcl::any` (`basic_any<tracked_ptr>`) on a stack or inside a managed object, `gc::any` (`basic_any<gc::tracked_ptr>`, [gc/gc.h](README.md#the-gc-namespace)) anywhere, a `std::map` of them included. What the `any` holds follows the rules of its type where the `any` lives, as a member would: an `sgcl::tracked_ptr` in a `gc::any` on the unmanaged heap is the mistake it would be in a `std::vector`; a value in a node is anywhere's.

## Rules

- An `sgcl::any` lives where a `tracked_ptr` may, a `gc::any` anywhere ([The rules](../README.md#the-rules), 1); what it holds follows the rules of its type where the `any` lives: an `sgcl::tracked_ptr` in a `gc::any` on the unmanaged heap is the mistake it would be as a member. A value in a node is fine anywhere.
- A pointer in the word is destroyed by `reset`, an assignment or the destructor: its object is unreferenced from then on. A value in a node is destroyed at the same moment, its destructor on the calling thread; the node goes back with the next sweep.
- A value in a node is traced: one that points back at the object holding the `any` is a cycle, collected when nothing else reaches it.
- `any_cast<T&>` on a value in a node is a reference into that node: valid while the `any` holds the value.
- Thread safety is that of `std::any` ([The rules](../README.md#the-rules), 6).

## Members

```cpp
basic_any() noexcept;
basic_any(const basic_any&);
basic_any(basic_any&&) noexcept;
template<class T> basic_any(T&& value);                                   // decay_t<T> copy constructible, not an any
template<class T, class... A> explicit basic_any(std::in_place_type_t<T>, A&&...);
template<class T, class U, class... A> explicit basic_any(std::in_place_type_t<T>, std::initializer_list<U>, A&&...);
~basic_any();

basic_any& operator=(const basic_any&);
basic_any& operator=(basic_any&&) noexcept;
template<class T> basic_any& operator=(T&& value);

template<class T, class... A> std::decay_t<T>& emplace(A&&...);
template<class T, class U, class... A> std::decay_t<T>& emplace(std::initializer_list<U>, A&&...);
void reset() noexcept;
void swap(basic_any&) noexcept;
bool has_value() const noexcept;
const std::type_info& type() const noexcept;
```

The free functions, in `sgcl`:

```cpp
template<template<class> class Ptr> void swap(basic_any<Ptr>&, basic_any<Ptr>&) noexcept;
template<class T, class... A> any make_any(A&&...);                              // an sgcl::any; gc::make_any makes a gc::any
template<class T, class U, class... A> any make_any(std::initializer_list<U>, A&&...);
template<class T, template<class> class Ptr> T any_cast(const basic_any<Ptr>&);   // T, const T&: bad_any_cast on another type
template<class T, template<class> class Ptr> T any_cast(basic_any<Ptr>&);         // T, T&
template<class T, template<class> class Ptr> T any_cast(basic_any<Ptr>&&);        // T, T&&
template<class T, template<class> class Ptr> const T* any_cast(const basic_any<Ptr>*) noexcept;   // null on another type
template<class T, template<class> class Ptr> T* any_cast(basic_any<Ptr>*) noexcept;
```

`any_cast<T>` compares `typeid(T)` with the type held, as `std::any_cast` does. With `using namespace sgcl` and a `std` type among the arguments, `make_any` needs its namespace (`sgcl::make_any<T>(...)`): argument-dependent lookup finds `std::make_any` as well.

```cpp
struct Node { int value; };
struct Pair { sgcl::tracked_ptr<Node> node; int count; };

sgcl::any a = sgcl::tracked_ptr(sgcl::make_tracked<Node>(1));   // in the word
sgcl::any b = Pair{sgcl::make_tracked<Node>(2), 7};               // in a managed node of its own
sgcl::any c = 3;                                                  // in the buffer
assert(sgcl::any_cast<sgcl::tracked_ptr<Node>>(a)->value == 1);
assert(sgcl::any_cast<Pair&>(b).count == 7);
assert(*sgcl::any_cast<int>(&c) == 3 && sgcl::any_cast<double>(&c) == nullptr);
b.reset();                                                        // the Pair destroyed now; its Node dies with the next cycle, the node with it
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>
#include <map>
#include <string>

// Properties attached to objects by name, of any type, some of them
// tracked pointers to other objects: a std::map of gc::any, living on
// the unmanaged heap like any std container, its pointers followed. A
// property pointing back at its own object is a cycle: collected.
struct Node {
    int id;
    std::map<std::string, gc::any> properties;
};

int main() {
    gc::tracked_ptr a = gc::make_tracked<Node>(1);
    gc::tracked_ptr b = gc::make_tracked<Node>(2);
    a->properties["weight"] = 2.5;                              // in the buffer
    a->properties["label"] = std::string("first");              // in a managed node
    a->properties["peer"] = b;                                  // a gc::tracked_ptr in the word: b lives while a does
    a->properties["peers"] = gc::vector<gc::tracked_ptr<Node>>{b, a};   // a container, in a managed node; a inside: a cycle through a, collected with a
    b = nullptr;
    gc::collector::force_collect(true);                         // optional, for the demonstration only
    auto& peer = gc::any_cast<gc::tracked_ptr<Node>&>(a->properties["peer"]);
    std::cout << peer->id << " " << gc::any_cast<double>(a->properties["weight"]) << "\n";   // 2 2.5
    return 0;
}
```

## See also

- [variant](variant.md): the same for a closed set of alternatives; [tracked_ptr](tracked_ptr.md), [weak_ptr](weak_ptr.md), [unique_ptr](unique_ptr.md)
- README: [variant and any](../README.md#variant-and-any), [Pointer maps](../README.md#pointer-maps), [The rules](../README.md#the-rules)
- `tests/any.cpp`: every behaviour above, checked.
