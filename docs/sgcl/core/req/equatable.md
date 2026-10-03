[sgcl](../../README.md) › [core](../README.md) › [req](../req.md)

# sgcl::req::equatable

```cpp
#include "sgcl/core/req.h"   // or "sgcl/core.h"

namespace sgcl::req {
    template<class T>
    concept equatable;  // T has ==
}
```

A value whose objects can be compared for equality: `a == b` is valid for two `const T&` and gives something
convertible to `bool`. A container of the library has `==` from [mixin::equatable](../mixin/equatable.md) only
when its elements are equatable, so `vector<T>` satisfies it exactly when `T` does. That is exactly what the standard
containers ask of their elements for `==`; `std::equality_comparable` would also ask for a `!=`, which a type
with a converting `==` cannot always form.

The questions of [mixin::enumerable](../mixin/enumerable.md) that compare elements (`contains`, `index_of`,
`last_index_of`) and the `==` of the containers take part only for elements that satisfy it.

## Satisfied by

- `int`, `double`, a pointer, `std::string`, `string`, `std::pair` of equatable types;
- every container of the library whose elements are equatable;
- a struct with `==` of its own or defaulted.

Not by a struct without `==`.

## Notes

The requirement is structural, not nominal like the requirements of a range, because a value need not be the
library's: an `int` or a class of the program passes on what it can do.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct point {
    int x, y;
    bool operator==(const point&) const = default;
};

struct blob {
    int size;
};

template<req::equatable T>
size_t count_equal(const vector<T>& values, const T& value) {
    size_t n = 0;
    for (const T& e : values) {
        if (e == value) {
            ++n;
        }
    }
    return n;
}

int main() {
    vector numbers = {1, 2, 1, 3};
    vector<point> points = {{0, 0}, {1, 2}, {0, 0}};
    println("{} {}", count_equal(numbers, 1), count_equal(points, point{0, 0}));
    println("{} {}", req::equatable<point>, req::equatable<blob>);
}
```

Output:

```text
2 2
true false
```

## See also

- [comparable](comparable.md): a value with an order
- [mixin::equatable](../mixin/equatable.md): `==` of a container, by its elements
- [sgcl::req](../req.md)
