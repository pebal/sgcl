[sgcl](../../README.md) › [core](../README.md) › [range](README.md)

# sgcl::range\<It\>::size

```cpp
size_t size() const noexcept(/* see below */);
```

The number of elements, `std::ranges::distance(begin(), end())`: a subtraction for an iterator whose difference is
sized (random access by the C++20 concept, the counting iterator included), a walk from `begin()` to `end()` for any
other.

## Parameters

None.

## Return value

The number of elements; up to `SIZE_MAX` for a counting range over the whole of a 64-bit type, whose difference
wraps in the `std::ptrdiff_t` ([counting_iterator](../counting_iterator.md)).

## Complexity

Constant for a sized iterator; linear in the number of elements otherwise.

## Exceptions

What the subtraction of the iterators, or their copy, increment and comparison for the walk, throws. The function
is `noexcept` when those operations are: for a pointer and the counting iterator always, for the iterator of a
`std::list` not.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <list>

using namespace sgcl;

int main() {
    std::list<int> l = {1, 2, 3};
    range walked(l.begin(), l.end());  // a walk
    println("{} {}", range(10, 20).size(), walked.size());
    println("{} {}", noexcept(range(3).size()), noexcept(walked.size()));
}
```

Output:

```text
10 3
true false
```

## See also

- [empty](empty.md): checks whether the range is empty
- [sgcl::range\<It\>](README.md)
