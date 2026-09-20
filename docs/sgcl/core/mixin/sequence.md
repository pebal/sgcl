# sgcl::mixin::sequence

```cpp
#include "sgcl/core/mixin/sequence.h"   // or "sgcl/core/mixin/mixin.h", "sgcl/sgcl.h"

namespace sgcl::mixin {
    template<class Derived>
    class sequence;
}
```

`mixin::sequence<Derived>` gives a class the writes over a range whose elements can be assigned — `fill`, `reverse` — and declares that they can: `req::sequence<R>` is "R carries `mixin::sequence`", which is what the sorts of [mixin::ordered](ordered.md) ask for ([the mixins](README.md)). The mutable sequences carry it, `slice<T>` (not `slice<const T>`), and `range` over an iterator that writes; the immutable containers and the associative ones do not. Nothing here asks anything of the element.

## Rules

- `reverse` needs a bidirectional range (`req::bidirectional`); `list` and `forward_list` have a `reverse` of their own, on the nodes, which hides this one.
- Thread safety is the container's.

## Members

```cpp
void fill(const auto& value);       // every element assigned the value
void reverse() noexcept;            // the elements in the opposite order, in place
```

```cpp
sgcl::vector v = {1, 2, 3};
v.reverse();                        // 3 2 1
sgcl::slice<int> tail = v.as_slice(1);
tail.fill(0);                       // 3 0 0: the slice writes the vector's elements
static_assert(sgcl::req::sequence<sgcl::vector<int>> && !sgcl::req::sequence<sgcl::slice<const int>> && !sgcl::req::sequence<sgcl::im::vector<int>>);
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

// A sequence of one's own with the mixins it can honour: a ring of the
// last N values, a managed buffer under it; contains, max and sort come
// from the mixins, begin and end are all they ask for.
template<class T, size_t N>
class ring
: public sgcl::mixin::enumerable<ring<T, N>>
, public sgcl::mixin::random_access<ring<T, N>>
, public sgcl::mixin::bidirectional<ring<T, N>>
, public sgcl::mixin::ordered<ring<T, N>>
, public sgcl::mixin::sequence<ring<T, N>> {
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
    sgcl::vector<T> _values;
    size_t _next = 0;
};

int main() {
    ring<int, 4> last;
    for (int x : {3, 9, 1, 7, 5}) {
        last.push(x);                                // 5 9 1 7: the 3 overwritten
    }
    std::cout << last.max() << (last.contains(3) ? " with 3" : " without 3") << "\n";   // 9 without 3
    last.sort();
    last.for_each([](int x) { std::cout << x << " "; });   // 1 5 7 9
    std::cout << "\n";
    return last.max() == 9 && !last.contains(3) ? 0 : 1;
}
```

The output:

```
9 without 3
1 5 7 9 
```

## See also

- [the mixins and the requirements](README.md); [vector](../../containers/vector.md), [array](../../containers/array.md), [deque](../../containers/deque.md), [list](../../containers/list.md), [forward_list](../../containers/forward_list.md): the sequences that carry it
- `tests/containers/mixin::sequence.cpp`: the members of the sequences' mixins, checked on every sequence; `tests/core/mixin.cpp`
