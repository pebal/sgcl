[sgcl](../../README.md) › [core](../README.md) › [vector](README.md)

# sgcl::vector\<T\>::operator=

```cpp
vector& operator=(const vector& other);               // (1)
vector& operator=(vector&& other) noexcept;           // (2)
vector& operator=(std::initializer_list<T> ilist);    // (3)
```

Replaces the elements of the vector.

1. With copies of the elements of `other`, as [assign](assign.md) copies them: within the capacity the buffer
   stays, above it the copies are built in a fresh buffer. An assignment to itself does nothing.
2. Destroys the elements, then takes the buffer of `other` over; `other` is empty after. An assignment to itself
   does nothing.
3. With the elements of `ilist`, as [assign](assign.md) does.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the vector the elements are copied or taken from |
| `ilist` | the list the elements are copied from |

## Return value

`*this`.

## Complexity

- (1) Linear in `other.size()`, plus linear in `size()` for the elements destroyed.
- (2) Linear in `size()` for the elements destroyed.
- (3) Linear in `ilist.size()`, plus linear in `size()` for the elements destroyed.

## Exceptions

- (1), (3) `length_error` when the number of elements is above `max_size()`, and what the copy constructor and
  the copy assignment of `T` throw. When the vector takes a fresh buffer, it is as it was before the call;
  within the capacity it stays consistent, but the values already assigned have changed.
- (2) None.

## Notes

A move takes the buffer over without touching an element of `other`, as `std::vector` does; the buffer this
vector held before is left to the collector, never freed at once.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector a = {1, 2, 3};
    vector<int> b;
    b = a;
    a[0] = 10;
    println("{} {}", a, b);

    vector<int> c;
    c = std::move(a);
    println("{} {}", c, a.size());

    c = {7, 8};
    println("{}", c);
}
```

Output:

```text
[10, 2, 3] [1, 2, 3]
[10, 2, 3] 0
[7, 8]
```

## See also

- [assign](assign.md): assigns copies of a value or a range
- [swap](swap.md): swaps the contents of two vectors
- [sgcl::vector\<T\>](README.md)
