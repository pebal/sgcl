[sgcl](../../README.md) › [core](../README.md) › [req](../req.md)

# sgcl::req::random_access

```cpp
#include "sgcl/core/req.h"   // or "sgcl/core.h"

namespace sgcl::req {
    template<class R>
    concept random_access;  // bidirectional<R>, and R carries mixin::random_access
}
```

A range whose elements are reached by position in constant time: [bidirectional](bidirectional.md), and `R`
carries `mixin::random_access`, a mixin without methods that says of the container what
`std::random_access_iterator_tag` says of its iterator. `sort`, `binary_search` and `lower_bound` ask for it.

## Satisfied by

- `vector`, `array`, `dynamic_array`, `deque`, `immutable::vector`, `slice`, a `range` over random-access
  iterators, `range(n)`.

Not by `list`, `forward_list` or the maps and sets.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

auto middle(const req::random_access auto& r) {
    return r.begin()[(r.end() - r.begin()) / 2];
}

int main() {
    vector numbers = {1, 2, 3, 4, 5};
    deque<string> words = {"left", "centre", "right"};
    println("{} {} {}", middle(numbers), middle(words), middle(range(10)));
    println("{}", req::random_access<list<int>>);
}
```

Output:

```text
3 centre 5
false
```

## See also

- [the mixins](../mixin/README.md): `mixin::random_access`, the declaration without methods this requirement asks for
- [bidirectional](bidirectional.md), [contiguous](contiguous.md)
- [sgcl::req](../req.md)
