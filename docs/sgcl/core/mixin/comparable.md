[sgcl](../../README.md) › [core](../README.md) › [mixin](README.md)

# sgcl::mixin::comparable\<Derived\>

```cpp
#include "sgcl/core/mixin/comparable.h"   // or "sgcl/core.h"

namespace sgcl::mixin {
    template<class Derived>
    class comparable;
}
```

`mixin::comparable<Derived>` gives a class `<=>` between two of its values — lexicographic by the elements, what
`<=>` is on the standard sequences and the ordered associative containers — so that its values satisfy
`req::comparable` when their elements do ([the mixins](README.md)). The operator exists only for elements that
are ordered (`req::comparable`): by their `<=>`, or by a weak ordering built from their `<` alone, as the standard
containers do (synth-three-way). `<`, `>`, `<=`, `>=` follow from it.

Carried beside [mixin::equatable](equatable.md), which gives `==`, or beside an `==` of the container's own (the
immutable `vector` and `list`); a container whose iteration order is not a value (the hash containers) does not
carry it.

## Template parameters

| Parameter | Description |
|---|---|
| `Derived` | The class that carries the mixin and names itself as the argument (`class vector : public mixin::comparable<vector<T>>`). It gives `begin()` and `end()`, const, over its elements. |

## Member functions

| Function | Description |
|---|---|
| `(constructor)`, `(destructor)` | protected: the mixin exists only as a base |

## Non-member functions

| Function | Description |
|---|---|
| [operator\<=\>, operator\<, operator\<=, operator\>, operator\>=](comparable/operator_cmp.md) | compare two values lexicographically by their elements |

## Example

A class of your own carries the mixins it can honour and gives `begin()` and `end()`; the requirements see it as
they see `vector`. The library writes the bases one per line, in alphabetical order of the mixin's name, so that
a reader finds one in a fixed place and a list never has to say why it is ordered as it is:

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

template<class T>
class series
: public mixin::bidirectional<series<T>>
, public mixin::comparable<series<T>>
, public mixin::enumerable<series<T>>
, public mixin::equatable<series<T>>
, public mixin::ordered<series<T>>
, public mixin::random_access<series<T>>
, public mixin::sequence<series<T>> {
public:
    void add(T value) { _values.push_back(value); }
    auto begin() { return _values.begin(); }
    auto end() { return _values.end(); }
    auto begin() const { return _values.begin(); }
    auto end() const { return _values.end(); }

private:
    vector<T> _values;
};

int main() {
    series<int> s;
    for (int x : {4, 1, 3}) {
        s.add(x);
    }
    s.sort();
    println("{} {} {}", s.max(), s.contains(3), s.is_sorted());
    println("{}", req::ordered<series<int>>);

    series<int> t = s;
    t.add(0);
    println("{} {} {}", s < t, s == t, s == s);
}
```

Output:

```text
4 true true
true
true false true
```

## See also

- [req::comparable](../req/comparable.md): a value with an order: what this mixin declares, and what its `<=>`
  asks of the elements
- [the mixins and the requirements](README.md), [mixin::equatable](equatable.md), [mixin::ordered](ordered.md):
  the order of a range, which asks for comparable elements
- `tests/core/mixin.cpp`
