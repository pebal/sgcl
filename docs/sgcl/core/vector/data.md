[sgcl](../../README.md) › [core](../README.md) › [vector](../vector.md)

# sgcl::vector\<T\>::data

```cpp
T* data() noexcept;                // (1)
const T* data() const noexcept;    // (2)
```

Returns a plain pointer to the first element of the buffer. The elements lie contiguously from it,
`[data(), data() + size())`, so the pointer may be passed to any function that expects a pointer to an element
of an array: a C API, `std::copy_n`, a loop over a raw range.

## Parameters

None.

## Return value

A pointer to the first element; a null pointer when the vector has no buffer (a vector constructed empty, or
emptied by [shrink_to_fit](shrink_to_fit.md)).

## Complexity

Constant.

## Exceptions

None.

## Notes

The pointer keeps nothing alive. It is valid until the vector reallocates or is destroyed, as with
`std::vector`; a `tracked_ptr` may not be made from it, as it may not address an element of a buffer. A view of
the elements that keeps the buffer alive is a [slice](../slice.md), from [as_slice](as_slice.md).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <algorithm>

using namespace sgcl;

int main() {
    vector<char> buffer(5);
    std::copy_n("hello", 5, buffer.data());
    println("{}", string(buffer.data(), 5));

    vector<int> empty;
    println("{}", empty.data() == nullptr);
}
```

Output:

```text
hello
true
```

## See also

- [as_slice](as_slice.md): the elements as a slice that holds the buffer
- [begin, cbegin](begin.md): an iterator to the beginning
- [sgcl::vector\<T\>](../vector.md)
