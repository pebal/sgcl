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

A `std::input_iterator` over the values of a [generator](generator/README.md), for the range-for and the views of
`std::ranges`: what [begin](generator/begin.md) returns; [end](generator/end.md) is `std::default_sentinel`. It is
lazy: its first look at a value (`*it`, `it->` or the comparison with the end) runs the coroutine to its next
`co_yield`, as [next](generator/next.md) does, and gives the value as a `T&`, which may be moved from (see
[value](generator/value.md)); `++` only marks the value used, so that the next look runs the coroutine on. Once the
coroutine has ended, the iterator compares equal to `std::default_sentinel`.

## Rules

- Single pass, as the generator is: copies of an iterator share the generator, and the look of each runs it on.
- An iterator left after its `++` (a `break`, `std::views::take`) has run the coroutine no further: the next
  `begin()` goes on from the value after the last one looked at.
- `std::views::filter` looks for the next value its predicate takes in its own `++`: a filter followed by a
  `std::views::take` runs the coroutine to the first match after the last value taken, and that match is lost
  with the filter's iterator.
- The iterator is valid while the generator lives and is not moved.
- An exception the coroutine throws comes out of the look that runs it.

## Member types

| Type | Definition |
|---|---|
| `iterator_concept` | `std::input_iterator_tag` |
| `iterator_category` | `std::input_iterator_tag` |
| `value_type` | `T` |
| `difference_type` | `std::ptrdiff_t` |
| `pointer` | `T*` |
| `reference` | `T&` |

## Member functions

| Function | Description |
|---|---|
| (constructor) | `noexcept`: an iterator of no generator, equal to `std::default_sentinel` |
| `operator*` | the generator's `value()`, a `T&`, after running the coroutine to it when the iterator holds none |
| `operator->` | the address of the same value |
| `operator++` | `noexcept`: marks the value used, runs nothing. The postfix form returns nothing |
| `operator==` | with `std::default_sentinel`: runs the coroutine when the iterator holds no value, equal once it has ended |

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

- [begin](generator/begin.md), [end](generator/end.md): the iterator and the end of a generator
- [generator](generator/README.md): the coroutine type
