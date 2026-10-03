[sgcl](../../README.md) › [core](../README.md) › [req](README.md)

# sgcl::req::sequence

```cpp
#include "sgcl/core/req.h"   // or "sgcl/core.h"

namespace sgcl::req {
    template<class R>
    concept sequence;  // enumerable<R>, and R carries mixin::sequence
}
```

A range whose elements may be written in place: [enumerable](enumerable.md), and `R` carries
[mixin::sequence](../mixin/sequence/README.md), which gives it `fill` and `reverse`. The order of the elements is the
program's, not the container's, so `sort` asks for it too.

## Satisfied by

- the mutable sequences: `vector`, `array`, `dynamic_array`, `deque`, `list`, `forward_list`;
- `slice<T>` of a non-const `T`.

Not by `slice<const T>`, the immutable containers, or the sorted and hash containers, whose order is their own.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <algorithm>

using namespace sgcl;

void clamp_all(req::sequence auto& r, int low, int high) {
    for (auto& x : r) {
        x = std::clamp(x, low, high);
    }
}

int main() {
    vector numbers = {-5, 3, 12};
    clamp_all(numbers, 0, 10);
    deque<int> more = {20, 1};
    clamp_all(more, 0, 10);
    println("{} {}", numbers, more);
    println("{} {}", req::sequence<sorted_set<int>>, req::sequence<slice<const int>>);
}
```

Output:

```text
[0, 3, 10] [10, 1]
false false
```

## See also

- [enumerable](enumerable.md), [ordered](ordered.md)
- [mixin::sequence](../mixin/sequence/README.md): the members it gives
- [sgcl::req](README.md)
