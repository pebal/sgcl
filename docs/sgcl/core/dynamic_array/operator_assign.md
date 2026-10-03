[sgcl](../../README.md) › [core](../README.md) › [dynamic_array](README.md)

# sgcl::dynamic_array\<T\>::operator=

```cpp
dynamic_array& operator=(const dynamic_array& other);        // (1)
dynamic_array& operator=(dynamic_array&& other) noexcept;    // (2)
dynamic_array& operator=(std::initializer_list<T> ilist);    // (3)
```

Replaces the contents of the array.

1. A copy of `other`. When the sizes are equal, the elements are assigned in place and the array keeps its
   buffer; otherwise a copy of `other` is built in a buffer of its own and swapped in, and the old elements are
   destroyed.
2. Destroys the current elements and takes the buffer of `other` over; `other` is empty after.
3. The elements of `ilist`: a fresh array is built and swapped in, and the old elements are destroyed.

An assignment to itself, (1) or (2), does nothing. A buffer the array gave up is left to the collector.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the array the elements are copied or taken from |
| `ilist` | the list the elements are copied from |

## Return value

`*this`.

## Complexity

- (1) Linear in `size()` and `other.size()`.
- (2) Linear in `size()`, the destruction of the current elements; constant when they have no destructor.
- (3) Linear in `size()` and `ilist.size()`.

## Exceptions

- (1) What the copy assignment of `T` throws, when the sizes are equal; what its copy constructor throws, when they
  are not.
- (2) None.
- (3) What the copy constructor of `T` throws.

When an element is copied into a fresh buffer, an exception leaves the array as it was before the call. When
the elements are assigned in place, it leaves the elements before the one that threw assigned and the others as
they were.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    dynamic_array<int> a = {1, 2, 3};
    dynamic_array<int> b(3);
    int* buffer = b.data();

    b = a;  // the same size: assigned in place
    println("{} {}", b, b.data() == buffer);

    b = {7, 8};  // a new buffer of two
    println("{} {}", b, b.data() == buffer);

    a = std::move(b);
    println("{} {}", a, b.empty());
}
```

Output:

```text
[1, 2, 3] true
[7, 8] false
[7, 8] true
```

## See also

- [(constructor)](dynamic_array.md): constructs an array
- [swap](swap.md): swaps the contents
- [sgcl::dynamic_array\<T\>](README.md)
