[sgcl](../../../README.md) › [core](../../README.md) › [mixin](../README.md) › [lookup](../lookup.md)

# sgcl::mixin::lookup\<Derived\>::values_of

```cpp
/*(1)*/ template<class K>
        auto values_of(const K& key) noexcept(/* see below */)
            requires requires(Derived& d) { d.equal_range(key); };
/*(2)*/ template<class K>
        auto values_of(const K& key) const noexcept(/* see below */)
            requires requires(const Derived& d) { d.equal_range(key); };
```

Returns every value under `key` as a range: a view of the values of the map's `equal_range(key)`, which copies
nothing. It is what a multimap holds under a key; on a map with one value per key the range holds one value or
none. Exists where the map has `equal_range` for `key`: every map of core, not `immutable::map`.

1. The values may be changed through the view.
2. The values `const`.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key whose values to take |

## Return value

A view of the values under `key`, empty when there is none, in the order the map's `equal_range` gives them (for
`sorted_multimap`, the order of insertion).

## Complexity

One search of the map, the complexity of its `equal_range`; a walk of the view is linear in the number of values
under the key.

## Exceptions

What the map's `equal_range` with a `K` throws: a transparent hash, equality or comparison called with another type
than the key's; none with the key type, whose calls the map requires noexcept.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    sorted_multimap<int, string> names = {{1, "a"}, {2, "z"}, {1, "bc"}};
    size_t length = 0;
    for (const string& name : names.values_of(1)) {
        length += name.size();
    }
    println("{} {}", length, names.values_of(3).empty());

    for (string& name : names.values_of(1)) {
        name = name + "!";
    }
    println("{}", *names.get(1));
}
```

Output:

```text
3 true
a!
```

## See also

- [get](get.md): the first value under a key
- [values](values.md): all the values as a range
- [sgcl::mixin::lookup\<Derived\>](../lookup.md)
