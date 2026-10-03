[sgcl](../../README.md) › [core](../README.md) › [req](README.md)

# sgcl::req::ordered

```cpp
#include "sgcl/core/req.h"   // or "sgcl/core.h"

namespace sgcl::req {
    template<class R>
    concept ordered;  // enumerable<R>, R carries mixin::ordered, and its elements are comparable
}
```

A range whose elements have an order the program may ask about: [enumerable](enumerable.md), `R` carries
[mixin::ordered](../mixin/ordered/README.md), and the elements are [comparable](comparable.md). It gives `is_sorted`,
`binary_search`, `lower_bound`, `upper_bound`, `sorted_index_of` and `sort`, and with comparable elements `min`
and `max` of [mixin::enumerable](../mixin/enumerable/README.md) are there too.

## Satisfied by

- the sequences, `slice`, `immutable::vector` and `immutable::list` of comparable elements.

Not by the sorted containers (`sorted_set`), which keep their own order and answer by the key, nor by a range of
elements without an order (`vector<point>` for a `point` without `<`).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct point {
    int x, y;
};

void describe(const req::ordered auto& r) {
    println("max {}, sorted {}", r.max(), r.is_sorted());
}

int main() {
    vector numbers = {3, 9, 4};
    describe(numbers);
    describe(numbers.as_slice(0, 1));
    println("{} {}", req::ordered<sorted_set<int>>, req::ordered<vector<point>>);
}
```

Output:

```text
max 9, sorted false
max 3, sorted true
false false
```

## See also

- [comparable](comparable.md), [sequence](sequence.md)
- [mixin::ordered](../mixin/ordered/README.md): the members it gives
- [sgcl::req](README.md)
