[sgcl](../../README.md) › [core](../README.md) › [mixin](README.md)

# sgcl::mixin::enumerable\<Derived\>

```cpp
#include "sgcl/core/mixin/enumerable.h"   // or "sgcl/core.h"

namespace sgcl {
    inline constexpr size_t npos;             // the position that is no position (req.h)

    namespace mixin {
        template<class Derived>
        class enumerable;
    }
}
```

`mixin::enumerable<Derived>` gives a class the questions asked of the elements of a range — is there one like this,
where, how many, the smallest — as members, over the `begin()` and `end()` of `Derived`, and declares the class a
range of the library: `req::enumerable<R>` is "R carries `mixin::enumerable`" ([the mixins](README.md)). Every
container that iterates carries it, from `vector` to `sorted_map`, `immutable::list` and `slice`; a class of your
own does by deriving from it and giving `begin()` and `end()`.

What `std` gives as free algorithms over a pair of iterators (`std::ranges::find`, `count_if`, `min_element`) and
Go as functions of its `slices` package is here a member of every range: `v.contains(x)`, `v.count_of(pred)`,
`v.min()`. A search that finds nothing returns `npos`, the position that is no position (`index_of`,
`last_index_of`, `find_index`), or a null pointer (`find_if`; on a range of values, an empty `optional`).

## Rules

- A question that compares elements exists only for elements that compare: `contains`, `index_of`,
  `last_index_of` for `req::equatable` elements (`==`), `min()` and `max()` for `req::comparable` ones (`<`); the
  forms with a predicate or a comparator ask nothing of the element. On a `vector<T>` whose `T` has neither,
  `v.exists(pred)` is there and `v.contains(x)` is not.
- `min` and `max` on an empty range are undefined, as `front()` is; nothing is checked. They return a reference
  into the range, or a value where the iterator gives values (`range(n)`).
- A container with a better answer hides the mixin's: `set::contains` by the key, `sorted_set::min()` as
  `*begin()`.
- Each member is noexcept as far as what it calls is: the element's `==` or `<`, the function given and its copy.
- Thread safety is the container's: the members read the elements as the algorithms do.

## Template parameters

| Parameter | Description |
|---|---|
| `Derived` | The class that carries the mixin and names itself as the argument (`class vector : public mixin::enumerable<vector<T>>`). It gives `begin()` and `end()`, const and not, over its elements. |

## Member functions

| Function | Description |
|---|---|
| `(constructor)`, `(destructor)` | protected: the mixin exists only as a base |

#### Lookup

| Function | Description |
|---|---|
| [contains](enumerable/contains.md) | checks whether an element is equal to a value |
| [index_of](enumerable/index_of.md) | the position of the first element equal to a value |
| [last_index_of](enumerable/last_index_of.md) | the position of the last element equal to a value |
| [find_if](enumerable/find_if.md) | a pointer to the first element the predicate accepts |
| [find_index](enumerable/find_index.md) | the position of the first element the predicate accepts |

#### Predicates

| Function | Description |
|---|---|
| [exists](enumerable/exists.md) | checks whether the predicate accepts some element |
| [all](enumerable/all.md) | checks whether the predicate accepts every element |
| [count_of](enumerable/count_of.md) | the number of elements the predicate accepts |

#### Minimum and maximum

| Function | Description |
|---|---|
| [min](enumerable/min.md) | the smallest element |
| [max](enumerable/max.md) | the largest element |

#### Visiting

| Function | Description |
|---|---|
| [for_each](enumerable/for_each.md) | calls a function with every element |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

// A function over any range of the library: a set, a slice, a list, a
// vector; what it asks for is what the parameter says
size_t count_odd(const req::enumerable auto& r) {
    return r.count_of([](int x) { return x % 2 != 0; });
}

int main() {
    vector v = {1, 2, 3, 4, 5};
    sorted_set<int> s = {7, 8, 9};
    println("{} {}", count_odd(v), count_odd(s));
    println("{} {}", count_odd(v.as_slice(1, 3)), count_odd(range(10)));
}
```

Output:

```text
3 2
1 5
```

## See also

- [req::enumerable](../req/enumerable.md): a range of the library: what a function asks for to call these members
- [the mixins and the requirements](README.md); [mixin::ordered](ordered.md), the questions about the order of
  the whole range
- `tests/core/mixin.cpp`
