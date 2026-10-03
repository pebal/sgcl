[sgcl](../README.md) › [core](README.md)

# sgcl::any

```cpp
#include "sgcl/core/any.h"   // or "sgcl/core.h"

namespace sgcl {
    class any;

    using std::bad_any_cast;
}
```

`sgcl::any` is `std::any` for values that hold tracked pointers. `std::any` keeps a small value in a buffer inside
itself, where a `tracked_ptr` would share its word with the data of other values (the offset leaves the pointer map
by elimination: [Pointer maps](../../garbage_collector/overview.md#pointer-maps)), and a large one on the unmanaged
heap, where a `tracked_ptr` may not live. Here a value goes to one of three places by what it is:

- a pointer word (`tracked_ptr` of either kind, [weak_ptr](weak_ptr.md)) into a word of the `any` that holds null
  or an address and nothing else;
- a small value that cannot hold a pointer into a buffer of 16 bytes inside the `any`: at most 16 bytes, aligned to
  at most 8, moved without throwing, and either trivially copyable (which a value with a pointer word never is),
  trivially default constructible, smaller than a word or aligned under one, or a `std::reference_wrapper`;
- anything else (a struct with a `tracked_ptr` member, a container, a `string`, a `std::string`: whatever the
  collector cannot rule out) into a managed node of its own, held by a pointer in the same word and traced through
  its own pointer map, so that a value pointing back at the `any`'s owner is a cycle collected like any other.

The value is destroyed the moment the `any` drops it, on that thread, as a container destroys an erased element;
the node is reclaimed by the collector later. An `any` is 32 bytes, as `std::any` in libc++.

The interface is that of `std::any`: the constructors and `in_place_type`, `emplace`, `reset`, `swap`, `has_value`,
`type`, [any_cast](any/any_cast.md) in every form, [make_any](make_any.md), and `bad_any_cast`, the one of `std`.
What differs: a copy of a value in a node is a node of its own; a moved-from `any` is empty; a value larger than a
page is not supported. Go has no counterpart but the empty interface, `interface{}`, which a type switch reads as
`any_cast` does.

## Rules

- The `any`'s word is a `tracked_ptr`, so an `any` lives where one may, as the containers do: on a stack or inside
  a managed object ([The rules](README.md#the-rules), 1). What it holds follows the rules of its type where the
  `any` lives, as a member would; a value in a node is fine anywhere.
- A pointer in the word is destroyed by `reset`, an assignment or the destructor: its object is unreferenced from
  then on. A value in a node is destroyed at the same moment, its destructor on the calling thread; the node goes
  back with the next sweep.
- A value in a node is traced: one that points back at the object holding the `any` is a cycle, collected when
  nothing else reaches it.
- `any_cast<T&>` on a value in a node is a reference into that node: valid while the `any` holds the value.
- Thread safety is that of `std::any`: concurrent readers, or one writer, with the program's own synchronization
  ([The rules](README.md#the-rules), 6).

## Member functions

| Function | Description |
|---|---|
| [(constructor)](any/any.md) | constructs an `any`, empty or holding a value |
| `(destructor)` | destroys the value, if any; a node is left to the collector |
| [operator=](any/operator_assign.md) | assigns another `any` or a value |

#### Modifiers

| Function | Description |
|---|---|
| [emplace](any/emplace.md) | constructs a value in place, destroying the one held |
| [reset](any/reset.md) | destroys the value held |
| [swap](any/swap.md) | swaps the contents of two `any` objects |

#### Observers

| Function | Description |
|---|---|
| [has_value](any/has_value.md) | checks whether the `any` holds a value |
| [type](any/type.md) | the `typeid` of the value held |

## Non-member functions

| Function | Description |
|---|---|
| [any_cast](any/any_cast.md) | the value as a given type: a copy, a reference or a pointer |
| [swap](any/swap2.md) | swaps the contents of two `any` objects |

## Complexity

Every operation is constant. A value in the word or in the buffer costs no allocation; a value in a node costs one
managed allocation per construction and per copy.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

// Properties attached to objects by name, of any type, some of them tracked pointers to other
// objects: a sorted_map of any inside the managed object, its pointers followed
struct Node {
    int id;
    sorted_map<string, any> properties;
};

int main() {
    tracked_ptr a = make_tracked<Node>(1);
    tracked_ptr b = make_tracked<Node>(2);
    a->properties["weight"] = 2.5;  // in the buffer
    a->properties["label"] = string("first");  // a string: in a managed node of its own
    a->properties["peer"] = b;  // a tracked_ptr in the word: b lives while a does
    // a container in a managed node; a inside it is a cycle through a, collected with a
    a->properties["peers"] = vector<tracked_ptr<Node>>{b, a};
    b = nullptr;
    collector::force_collect(true);  // optional, for the demonstration only

    auto& peer = any_cast<tracked_ptr<Node>&>(a->properties["peer"]);
    println("{} {}", peer->id, any_cast<double>(a->properties["weight"]));
    println("{}", any_cast<string&>(a->properties["label"]));
}
```

Output:

```text
2 2.5
first
```

## See also

- [variant](variant.md): the same for a closed set of alternatives
- [make_any](make_any.md): an `any` with a value constructed in place
- [tracked_ptr](tracked_ptr.md), [weak_ptr](weak_ptr.md): the pointer words
- [function](function.md): the same storage for a closure
- [Pointer maps](../../garbage_collector/overview.md#pointer-maps), [README: The rules](README.md#the-rules)
