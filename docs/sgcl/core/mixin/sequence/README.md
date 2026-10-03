[sgcl](../../../README.md) › [core](../../README.md) › [mixin](../README.md)

# sgcl::mixin::sequence\<Derived\>

```cpp
#include "sgcl/core/mixin/sequence.h"   // or "sgcl/core.h"

namespace sgcl::mixin {
    template<class Derived>
    class sequence;
}
```

`mixin::sequence<Derived>` gives a class the writes over a range whose elements can be assigned — `fill`,
`reverse` — and declares that they can: `req::sequence<R>` is "R carries `mixin::sequence`", which is what the
sorts of [mixin::ordered](../ordered/README.md) ask for ([the mixins](../README.md)). The mutable sequences carry it,
`slice<T>` (not `slice<const T>`), and `range` over an iterator that writes; the immutable containers and the
associative ones do not. Nothing here asks anything of the element.

What `std` gives as the free algorithms `std::ranges::fill` and `std::ranges::reverse`, and Go as
`slices.Reverse`, is here a member of every sequence.

## Rules

- `reverse` needs a bidirectional range (`req::bidirectional`); `list` and `forward_list` have a `reverse` of
  their own, on the nodes, which hides this one.
- Each method is noexcept as far as what it calls is: the assignment of the value to an element, the swap of the
  elements.
- Thread safety is the container's.

## Template parameters

| Parameter | Description |
|---|---|
| `Derived` | The class that carries the mixin and names itself as the argument (`class vector : public mixin::sequence<vector<T>>`). It gives `begin()` and `end()` over elements that can be written. |

## Member functions

| Function | Description |
|---|---|
| `(constructor)`, `(destructor)` | protected: the mixin exists only as a base |

#### Modifiers

| Function | Description |
|---|---|
| [fill](fill.md) | assigns a value to every element |
| [reverse](reverse.md) | reverses the order of the elements |

## Example

A sequence of one's own carries the mixins it can honour: here a ring of the last N values over a managed
buffer, whose `contains`, `max` and `sort` come from the mixins, on `begin()` and `end()` alone.

```cpp
#include "sgcl/core.h"
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

template<class T, size_t N>
class ring
: public mixin::bidirectional<ring<T, N>>
, public mixin::enumerable<ring<T, N>>
, public mixin::ordered<ring<T, N>>
, public mixin::random_access<ring<T, N>>
, public mixin::sequence<ring<T, N>> {
public:
    void push(const T& value) {
        if (_values.size() < N) {
            _values.push_back(value);
        } else {
            _values[_next] = value;
        }
        _next = (_next + 1) % N;
    }

    auto begin() { return _values.begin(); }
    auto end() { return _values.end(); }
    auto begin() const { return _values.begin(); }
    auto end() const { return _values.end(); }

private:
    vector<T> _values;
    size_t _next = 0;
};

int main() {
    ring<int, 4> last;
    for (int x : {3, 9, 1, 7, 5}) {
        last.push(x);  // the fifth overwrites the first
    }
    println("{}{}", last.max(), (last.contains(3) ? " with 3" : " without 3"));
    last.sort();
    last.for_each([](int x) { print("{} ", x); });
    println();

    println("{} {}", req::sequence<ring<int, 4>>, req::sequence<vector<int>>);
    println("{} {}", req::sequence<slice<const int>>, req::sequence<immutable::vector<int>>);
}
```

Output:

```text
9 without 3
1 5 7 9 
true true
false false
```

## See also

- [req::sequence](../../req/sequence.md): a range written in place: what a function asks for to call these members
- [the mixins and the requirements](../README.md); [vector](../../vector/README.md), [array](../../array/README.md),
  [deque](../../deque/README.md), [list](../../list/README.md), [forward_list](../../forward_list/README.md): the sequences that carry it
- `tests/containers/mixins.cpp`: the members of the sequences' mixins, checked on every sequence;
  `tests/core/mixin.cpp`
