[sgcl](../../../README.md) › [core](../../README.md) › [mixin](../README.md)

# sgcl::mixin::equatable\<Derived\>

```cpp
#include "sgcl/core/mixin/equatable.h"   // or "sgcl/core.h"

namespace sgcl::mixin {
    template<class Derived>
    class equatable;
}
```

`mixin::equatable<Derived>` gives a class `==` between two of its values — equal when they hold equal elements in
the same order, what `==` is on every standard container — so that its values satisfy `req::equatable` when their
elements do ([the mixins](../README.md)). The operator is a friend of the base, found through `Derived`, and exists
only for elements that compare (`req::equatable` elements); `!=` follows from it.

Nothing about order: that is [mixin::comparable](../comparable/README.md), carried beside this one. A container whose
iteration order is not a value carries neither and gives an `==` of its own (the hash containers: the same
elements in any order), and so do the immutable ones (a version and its copy are equal by their structure before
any element is read).

## Template parameters

| Parameter | Description |
|---|---|
| `Derived` | The class that carries the mixin and names itself as the argument (`class vector : public mixin::equatable<vector<T>>`). It gives `begin()` and `end()`, const, over its elements. |

## Member functions

| Function | Description |
|---|---|
| `(constructor)`, `(destructor)` | protected: the mixin exists only as a base |

## Non-member functions

| Function | Description |
|---|---|
| [operator==, operator!=](operator_cmp.md) | compare two values by their elements |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

// A class of one's own: == by the elements, from the mixin
class route
: public mixin::enumerable<route>
, public mixin::equatable<route> {
public:
    route(std::initializer_list<string> stops) : _stops(stops) {}

    auto begin() const { return _stops.begin(); }
    auto end() const { return _stops.end(); }

private:
    vector<string> _stops;
};

int main() {
    route a = {"home", "work"}, b = {"home", "work"}, c = {"work", "home"};
    println("{} {}", a == b, a == c);
    println("{} {}", req::equatable<route>, req::comparable<route>);
}
```

Output:

```text
true false
true false
```

## See also

- [req::equatable](../../req/equatable.md): a value with `==`: what this mixin declares, and what its `==` asks of
  the elements
- [the mixins and the requirements](../README.md), [mixin::comparable](../comparable/README.md)
- `tests/core/mixin.cpp`
