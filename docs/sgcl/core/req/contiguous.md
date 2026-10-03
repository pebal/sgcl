[sgcl](../../README.md) › [core](../README.md) › [req](README.md)

# sgcl::req::contiguous

```cpp
#include "sgcl/core/req.h"   // or "sgcl/core.h"

namespace sgcl::req {
    template<class R>
    concept contiguous;  // random_access<R>, and R carries mixin::contiguous
}
```

A range whose elements lie next to each other in memory: [random_access](random_access.md), and `R` carries
`mixin::contiguous`, a mixin without methods that says of the container what `std::contiguous_iterator_tag` says
of its iterator. Its elements are an array: `data()` is a pointer to the first one, and a function that takes a
pointer and a count takes them.

## Satisfied by

- `vector`, `array`, `dynamic_array`, `slice`.

Not by `deque`, whose elements lie in blocks, nor by `immutable::vector`, whose elements lie in a tree.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int sum_of(const int* values, size_t count) {  // a function of a C library
    int sum = 0;
    for (size_t i : range(count)) {
        sum += values[i];
    }
    return sum;
}

int sum(const req::contiguous auto& r) {
    return sum_of(r.data(), size_t(r.end() - r.begin()));
}

int main() {
    vector numbers = {1, 2, 3, 4};
    array<int, 3> fixed = {10, 20, 30};
    println("{} {} {}", sum(numbers), sum(fixed), sum(numbers.as_slice(2)));
    println("{}", req::contiguous<deque<int>>);
}
```

Output:

```text
10 60 7
false
```

## See also

- [the mixins](../mixin/README.md): `mixin::contiguous`, the declaration without methods this requirement asks for
- [random_access](random_access.md)
- [slice](../slice/README.md): a view of contiguous elements
- [sgcl::req](README.md)
