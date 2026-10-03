[sgcl](../../README.md) › [core](../README.md) › [deque](../deque.md)

# sgcl::deque\<T\>::operator=

```cpp
/*(1)*/ deque& operator=(const deque& other);
/*(2)*/ deque& operator=(deque&& other) noexcept;
/*(3)*/ deque& operator=(std::initializer_list<T> ilist);
```

Replaces the elements of the deque.

1. With copies of the elements of `other`: `assign(other.begin(), other.end())`, in blocks of this deque's own.
   An assignment to itself does nothing.
2. Destroys the elements, as [clear](clear.md) does, then takes the map of `other` over; `other` is empty after.
   An assignment to itself does nothing.
3. With the elements of `ilist`, as [assign](assign.md) does.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the deque the elements are copied or taken from |
| `ilist` | the list the elements are copied from |

## Return value

`*this`.

## Complexity

- (1) Linear in `other.size()`, plus linear in `size()` for the elements destroyed.
- (2) Linear in `size()` for the elements destroyed.
- (3) Linear in `ilist.size()`, plus linear in `size()` for the elements destroyed.

## Exceptions

- (1), (3) What the copy constructor and the copy assignment of `T` throw. The deque stays consistent and every
  element is destroyed exactly once, but the values already assigned have changed and the elements appended
  before the throw stay.
- (2) None.

## Notes

A move takes the map over without touching an element of `other`, as `std::deque` does; the blocks and the map
this deque held before are left to the collector, never freed at once.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    deque a = {1, 2, 3};
    deque<int> b;
    b = a;
    a[0] = 10;
    println("{} {}", a, b);

    deque<int> c;
    c = std::move(a);
    println("{} {}", c, a.size());

    c = {4, 5};
    println("{}", c);
}
```

Output:

```text
[10, 2, 3] [1, 2, 3]
[10, 2, 3] 0
[4, 5]
```

## See also

- [assign](assign.md): assigns copies of a value or a range
- [swap](swap.md): swaps the contents of two deques
- [sgcl::deque\<T\>](../deque.md)
