[sgcl](../../README.md) › [core](../README.md) › [req](../req.md)

# sgcl::req::bidirectional

```cpp
#include "sgcl/core/req.h"   // or "sgcl/core.h"

namespace sgcl::req {
    template<class R>
    concept bidirectional;  // enumerable<R>, and R carries mixin::bidirectional
}
```

A range that can be walked backwards: [enumerable](enumerable.md), and `R` carries `mixin::bidirectional`, a
mixin without methods that says of the container what `std::bidirectional_iterator_tag` says of its iterator.
`reverse` and the reverse iterators ask for it.

## Satisfied by

- `vector`, `array`, `dynamic_array`, `deque`, `list`, `sorted_map`, `sorted_set` and their multi forms,
  `slice`, a `range` over bidirectional iterators.

Not by `forward_list`, nor by the hash containers `map` and `set`.

## Notes

Each category implies the ones below it: [contiguous](contiguous.md) is [random_access](random_access.md) is
`bidirectional` is [enumerable](enumerable.md).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

void print_backwards(const req::bidirectional auto& r) {
    auto it = r.end();
    print("{}", *--it);
    while (it != r.begin()) {
        print(" {}", *--it);
    }
    println();
}

int main() {
    vector numbers = {1, 2, 3};
    list<string> words = {"a", "b"};
    sorted_set<int> keys = {5, 3, 4};
    print_backwards(numbers);
    print_backwards(words);
    print_backwards(keys);
    println("{} {}", req::bidirectional<forward_list<int>>, req::bidirectional<set<int>>);
}
```

Output:

```text
3 2 1
b a
5 4 3
false false
```

## See also

- [the mixins](../mixin/README.md): `mixin::bidirectional`, the declaration without methods this requirement asks for
- [enumerable](enumerable.md), [random_access](random_access.md)
- [sgcl::req](../req.md)
