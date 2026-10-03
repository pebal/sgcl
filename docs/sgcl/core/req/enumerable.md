[sgcl](../../README.md) › [core](../README.md) › [req](README.md)

# sgcl::req::enumerable

```cpp
#include "sgcl/core/req.h"   // or "sgcl/core.h"

namespace sgcl::req {
    template<class R>
    concept enumerable;  // R carries mixin::enumerable
}
```

A range of the library: `R` carries [mixin::enumerable](../mixin/enumerable/README.md), which gives it `contains`,
`index_of`, `find_if`, `count_of`, `min`, `max` and `for_each` as members. It is the requirement every other
requirement of a range builds on.

The requirement is nominal: it asks whether the type declared itself by carrying the mixin, not whether it has
`begin()` and `end()`. A `std::vector` iterates but does not satisfy it; a function that takes
`const req::enumerable auto&` refuses it at the call, in one line.

## Satisfied by

- every container of the library that iterates: `vector`, `array`, `dynamic_array`, `deque`, `list`,
  `forward_list`, the maps and sets, the immutable containers;
- [slice](../slice/README.md) and [range](../range/README.md).

Not by a `std` container, and not by `string`. A `std` container becomes one through an adapter:
`range(v.begin(), v.end())`, or `slice(v)` for contiguous memory.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <vector>

using namespace sgcl;

size_t count_odd(const req::enumerable auto& r) {
    return r.count_of([](int x) { return x % 2 != 0; });
}

int main() {
    vector numbers = {1, 2, 3};
    list<int> more = {5, 7};
    println("{} {} {}", count_odd(numbers), count_odd(more), count_odd(range(10)));

    std::vector<int> plain = {1, 3};
    println("{} {}", req::enumerable<std::vector<int>>, count_odd(range(plain.begin(), plain.end())));
}
```

Output:

```text
2 2 5
false 2
```

## See also

- [bidirectional](bidirectional.md), [sequence](sequence.md), [ordered](ordered.md), [lookup](lookup.md): what builds on it
- [mixin::enumerable](../mixin/enumerable/README.md): the members it gives
- [sgcl::req](README.md)
