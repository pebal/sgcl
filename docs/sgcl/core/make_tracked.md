[sgcl](../README.md) › [core](README.md)

# sgcl::make_tracked\<T\>

```cpp
#include "sgcl/core/make_tracked.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class T, class ...A>
    auto make_tracked(A&&... a) noexcept(/* see below */);
}
```

Creates an object of type `T` on the managed heap, constructed from `a...`, and returns it as a
[unique_ptr](unique_ptr/README.md)`<T>`. It is the one way an object enters the managed heap (the containers make their
nodes and buffers the same way, internally), and the counterpart of `std::make_unique`: deterministic ownership
until the `unique_ptr` is moved into a [tracked_ptr](tracked_ptr/README.md), after which the collector owns the object and
destroys it when nothing reaches it, or until the `unique_ptr` is dropped, which destroys the object at once. In Go
the same is `new(T)` or `&T{...}`, whose object is the collector's from the start.

The object is constructed as `new (slot) T(std::forward<A>(a)...)`, or `new (slot) T` with no arguments:
default-initialization, like `std::make_unique_for_overwrite`. A trivial type is then left uninitialized: the slot
holds whatever the last object of the type left there, null at the pointer offsets. For zeros, write the value:
`make_tracked<T>(T{})`. Aggregates take their arguments in parentheses (C++20).

`T` is an object type: not an array (`make_tracked<T[]>` is a compile error: managed arrays are not a public type,
`sgcl::vector` and `sgcl::dynamic_array<T>` own theirs), not `void`, and not larger than a page
(`config::page_size`, 64 KB, less a 16-byte header; a compile error past that). Large data goes into a container.
`T` may be `const`; it may hold `tracked_ptr`s, `weak_ptr`s, `unique_ptr`s, containers and atomics, since a managed
object is where all of those may live. It may be a `tracked_ptr` itself: `make_tracked<tracked_ptr<T>>()` is the
managed word a root from outside the managed world holds
([Stack roots](../../garbage_collector/overview.md#stack-roots)).

The function is `noexcept` when the constructor of `T` that the arguments select is: the test is the `noexcept` of
the placement-new expression, so a private constructor the library may call counts as well.

## Parameters

| Parameter | Description |
|---|---|
| `a` | the arguments of the constructor of `T`, forwarded |

## Return value

A `unique_ptr<T>` owning the new object.

## Complexity

Constant: a slot from a thread-local pool of the object's size class, no lock, no wait for the collector, and no
header on the object (the metadata, the type included, is the page's), plus the constructor of `T`.

## Exceptions

What the constructor of `T` throws; none when it is noexcept. A constructor that throws gives the slot back and lets
the exception through.

## Notes

The constructor of `T` runs on the calling thread, at once; the destructor runs when the owner destroys the object
(a `unique_ptr`, on its thread) or when the collector does (a `tracked_ptr`, on a collector thread, under the rules
of destructors: [The rules](README.md#the-rules), 5).

The collector may read the words of the slot while the constructor runs. The words at the type's pointer offsets
are null then (a page is zero when it is issued to a type, and every object of the type that died in the slot since
left its pointers null: the destructors of `tracked_ptr` and `unique_ptr` store a null) and the constructor's stores
land one by one, which is why a `tracked_ptr` member initialized in the constructor is a root from its store on.

Any thread may call it; the first managed object a thread creates registers the thread with the collector, and the
first one in the program starts the collector ([Threads](../async/README.md#threads)).

When the committed memory would cross the ceiling (`collector::get_memory_limit()`), the call first forces a full
collection and waits for it; if that does not free enough, the program ends with one line on stderr
(`sgcl: out of managed memory: …`) and `std::terminate()`. A destructor run by the sweep that allocates past the
ceiling ends the program at once
([collector](collector/README.md#the-memory-limit),
[Memory](../../garbage_collector/overview.md#memory)). The allocation itself throws nothing.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Point {
    int x, y;
};

struct Node {
    int value;
    tracked_ptr<Node> next;
};

struct Label {
    Label(string text, tracked_ptr<Point> at) : text(std::move(text)), at(at) {}
    string text;
    tracked_ptr<Point> at;  // stored by the constructor: a root from that store on
};

int main() {
    unique_ptr number = make_tracked<int>(42);
    unique_ptr point = make_tracked<Point>(1, 2);    // an aggregate, in parentheses
    tracked_ptr node = make_tracked<Node>(7);        // the collector's from now on
    node->next = make_tracked<Node>(8, node);        // a cycle
    unique_ptr bare = make_tracked<Node>();          // default-initialized: next is null
    unique_ptr zeroed = make_tracked<Node>(Node{});  // value-initialized: value is 0 too
    tracked_ptr label = make_tracked<Label>("origin", make_tracked<Point>(0, 0));

    println("{} {} {}", *number, point->y, node->next->next == node);
    println("{} {}", bare->next == nullptr, zeroed->value);
    println("{} at {},{}", label->text, label->at->x, label->at->y);
}
```

Output:

```text
42 2 true
true 0
origin at 0,0
```

## See also

- [unique_ptr](unique_ptr/README.md): the owner it returns
- [tracked_ptr](tracked_ptr/README.md): the pointer the collector follows
- [weak_ptr](weak_ptr/README.md): a pointer that keeps nothing alive
- [vector](vector/README.md), [array](array/README.md): managed sequences, which take the place of managed arrays
- [collector](collector/README.md), [config](config.md): the memory ceiling and the page size
- [README: Pointers](README.md#pointers), [README: The rules](README.md#the-rules),
  [Memory](../../garbage_collector/overview.md#memory)
