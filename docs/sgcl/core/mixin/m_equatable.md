# sgcl::m_equatable

```cpp
#include "sgcl/core/mixin/m_equatable.h"   // or "sgcl/core/mixin/mixin.h", "sgcl/sgcl.h"

namespace sgcl {
    template<class Derived>
    class m_equatable;
}
```

`m_equatable<Derived>` gives a class `==` between two of its values — equal when they hold equal elements in the same order, what `==` is on every standard container — and declares that its values compare: `c_equatable<R>` is "R carries `m_equatable`", or, for a value that is not the library's, "R has `==`" ([the mixins](README.md)). The operator is a friend of the base, found through `Derived`, and exists only for elements that compare (`c_equatable` elements); `!=` follows from it. Nothing about order: that is [m_comparable](m_comparable.md), and a container whose iteration order is not a value (`set`) carries only this one, or gives an `==` of its own (the hash containers, the immutable ones: a version and its copy are equal by their structure before any element is read).

```cpp
sgcl::vector a = {1, 2}, b = {1, 2}, c = {2, 1};
assert(a == b && a != c);
sgcl::list<int> l = {1, 2}, m = {1, 2};
assert(l == m);
struct point { int x, y; };
sgcl::vector<point> p, q;
// p == q: does not exist — point has no ==
static_assert(sgcl::c_equatable<sgcl::vector<int>> && !sgcl::c_equatable<point>);
```

## See also

- [the mixins and the concepts](README.md), [m_comparable](m_comparable.md)
