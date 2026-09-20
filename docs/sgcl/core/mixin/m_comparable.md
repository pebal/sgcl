# sgcl::m_comparable

```cpp
#include "sgcl/core/mixin/m_comparable.h"   // or "sgcl/core/mixin/mixin.h", "sgcl/sgcl.h"

namespace sgcl {
    template<class Derived>
    class m_comparable;
}
```

`m_comparable<Derived>` gives a class `<=>` between two of its values — lexicographic by the elements, what `<=>` is on the standard sequences and the ordered associative containers — and declares that its values are ordered: `c_comparable<R>` is "R carries `m_comparable`", or, for a value that is not the library's, "R has `<=>` or `<`" ([the mixins](README.md)). The operator exists only for elements that are ordered (`c_comparable`): by their `<=>`, or by a weak ordering built from their `<` alone, as the standard containers do (synth-three-way). `<`, `>`, `<=`, `>=` follow from it. Carried beside [m_equatable](m_equatable.md), which gives `==`; a container whose iteration order is not a value does not carry it.

```cpp
sgcl::vector a = {1, 2}, b = {1, 3};
assert(a < b && (a <=> b) < 0);
struct less_only { int v; bool operator<(less_only o) const { return v < o.v; } };
sgcl::vector<less_only> x = {{1}}, y = {{2}};
assert(x < y);                                                              // by < alone
static_assert(std::is_same_v<decltype(x <=> y), std::weak_ordering>);       // as the standard: weak
struct opaque { int v; };                                                   // neither <=> nor <
static_assert(sgcl::c_comparable<less_only> && sgcl::c_comparable<std::pair<int, int>> && !sgcl::c_comparable<opaque>);
```

## See also

- [the mixins and the concepts](README.md), [m_equatable](m_equatable.md), [m_ordered](m_ordered.md): the order of a range, which asks for comparable elements
