[sgcl](../README.md) › [core](README.md) › [generator](generator/README.md)

# sgcl::generator\<T\>::iterator

```cpp
#include "sgcl/core/generator.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class T>
    class generator {
    public:
        class iterator;
    };
}
```

An input iterator over the values of a [generator](generator/README.md), for the range-for: what
[begin](generator/begin.md) and [end](generator/end.md) return. It holds a pointer to the generator, null at the
end. Dereferencing gives the current value as a `T&`, which may be moved from (see [value](generator/value.md));
`++` runs the coroutine to its next `co_yield` and turns into the end iterator when the coroutine ends.

## Rules

- Single pass, as the generator is: copies of an iterator share the generator, and `++` on one advances all.
- The iterator is valid while the generator lives and is not moved.
- An exception the coroutine throws comes out of `++`.

## Member types

| Type | Definition |
|---|---|
| `iterator_category` | `std::input_iterator_tag` |
| `value_type` | `T` |
| `difference_type` | `std::ptrdiff_t` |
| `pointer` | `T*` |
| `reference` | `T&` |

## Member functions

| Function | Description |
|---|---|
| (constructor) | `noexcept`: the end iterator |
| `operator*` | `noexcept`: the generator's `value()`, a `T&` |
| `operator->` | `noexcept`: the address of the generator's `value()` |
| `operator++` | `next()`; the iterator becomes the end iterator when it returns `false`. The postfix form returns nothing |
| `operator==`, `operator!=` | `noexcept`: equal when both refer to the same generator or both are the end |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

generator<int> squares(int n) {
    for (int i : range(1, n + 1)) {
        co_yield i * i;
    }
}

int main() {
    generator<int> g = squares(3);
    for (auto it = g.begin(); it != g.end(); ++it) {
        println("{}", *it);
    }
}
```

Output:

```text
1
4
9
```

## See also

- [begin](generator/begin.md), [end](generator/end.md): the iterators of a generator
- [generator](generator/README.md): the coroutine type
