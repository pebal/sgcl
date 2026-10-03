[sgcl](../../README.md) › [core](../README.md) › [req](../req.md)

# sgcl::req::comparable

```cpp
#include "sgcl/core/req.h"   // or "sgcl/core.h"

namespace sgcl::req {
    template<class T>
    concept comparable;  // T has <=>, or < alone
}
```

A value with an order: `T` is `std::three_way_comparable`, or `a < b` is valid for two `const T&` and gives
something convertible to `bool`. A container of the library has `<=>` from
[mixin::comparable](../mixin/comparable.md) only when its elements are comparable.
The last is what the standard containers ask of their elements for their `<=>`, synthesized from `<` when `T` has
no `<=>`.

`min`, `max`, `sort` without a comparator, `is_sorted`, `binary_search` and the `<=>` of the containers take part
only for elements that satisfy it.

## Satisfied by

- `int`, `double`, `std::string`, `string`, `std::pair` of comparable types;
- every container of the library whose elements are comparable;
- a struct with `<=>`, or with `<` alone.

Not by a struct with neither.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct version {
    int major, minor;
    auto operator<=>(const version&) const = default;
};

struct ticket {
    int id;
    bool operator<(const ticket& other) const { return id < other.id; }
};

struct blob {
    int size;
};

template<req::comparable T>
const T& larger(const T& a, const T& b) {
    return a < b ? b : a;
}

int main() {
    println("{}", larger(version{1, 2}, version{1, 10}).minor);
    println("{}", larger(ticket{3}, ticket{7}).id);
    println("{}", larger(string("pear"), string("apple")));
    println("{} {} {}", req::comparable<version>, req::comparable<ticket>, req::comparable<blob>);
}
```

Output:

```text
10
7
pear
true true false
```

## See also

- [equatable](equatable.md): a value with `==`
- [ordered](ordered.md): a range of comparable elements
- [mixin::comparable](../mixin/comparable.md): `<=>` of a container, by its elements
- [sgcl::req](../req.md)
