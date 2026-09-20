# sgcl::mixin::equatable

```cpp
#include "sgcl/core/mixin/equatable.h"   // or "sgcl/core/mixin/mixin.h", "sgcl/sgcl.h"

namespace sgcl::mixin {
    template<class Derived>
    class equatable;
}
```

`mixin::equatable<Derived>` gives a class `==` between two of its values — equal when they hold equal elements in the same order, what `==` is on every standard container — and declares that its values compare: `req::equatable<R>` is "R carries `mixin::equatable`", or, for a value that is not the library's, "R has `==`" ([the mixins](README.md)). The operator is a friend of the base, found through `Derived`, and exists only for elements that compare (`req::equatable` elements); `!=` follows from it. Nothing about order: that is [mixin::comparable](comparable.md), and a container whose iteration order is not a value (`set`) carries only this one, or gives an `==` of its own (the hash containers, the immutable ones: a version and its copy are equal by their structure before any element is read).

```cpp
vector a = {1, 2}, b = {1, 2}, c = {2, 1};
assert(a == b && a != c);
list<int> l = {1, 2}, m = {1, 2};
assert(l == m);
struct point { int x, y; };
vector<point> p, q;
// p == q: does not exist — point has no ==
static_assert(req::equatable<vector<int>> && !req::equatable<point>);
```

## See also

- [the mixins and the requirements](README.md), [mixin::comparable](comparable.md)
