[sgcl](../../README.md) › [core](../README.md) › [tracked_ptr](../tracked_ptr.md)

# sgcl::tracked_ptr\<T\>::shade

```cpp
void shade() const noexcept;
```

The write barrier for the target, on demand: the target is made reachable in the current cycle, its state and its
card left as a store of this pointer would leave them. On a pointer just made from a raw address it is the barrier
that construction ran, once more.

It completes a copy made with [store](store.md) without the barrier: once every word of the copy is stored, one
`shade()` of a pointer to the source makes the source reachable in this cycle, and the marking, visiting it, marks
every child the copy holds.

## Parameters

None.

## Return value

None.

## Complexity

Constant.

## Exceptions

None.

## Notes

The shade comes after the copy is complete, never before (the copies of the immutable containers made with a shade
of the root ahead of them lost nodes), and the source is held by the caller through the copy and the shade.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Pair {
    Pair(tracked_ptr<string> first, tracked_ptr<string> second) : first(first), second(second) {}
    Pair(const Pair& from) noexcept
    : first(from.first, barrier::off)
    , second(from.second, barrier::off) {
    }
    tracked_ptr<string> first, second;
};

unique_ptr<Pair> copy_of(const tracked_ptr<Pair>& source) {
    unique_ptr<Pair> copy = make_tracked<Pair>(*source);
    source.shade();  // after the copy is complete
    return copy;
}

int main() {
    tracked_ptr source = make_tracked<Pair>(make_tracked<string>("a"), make_tracked<string>("b"));
    tracked_ptr copy = copy_of(source);
    println("{} {}", *copy->first, *copy->second);
}
```

Output:

```text
a b
```

## See also

- [store](store.md): the store without the barrier
- [sgcl::tracked_ptr\<T\>](../tracked_ptr.md)
