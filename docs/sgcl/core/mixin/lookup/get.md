[sgcl](../../../README.md) › [core](../../README.md) › [mixin](../README.md) › [lookup](README.md)

# sgcl::mixin::lookup\<Derived\>::get

```cpp
template<class K>
auto get(const K& key) const noexcept(/* see below */);
```

Returns a copy of the value under `key` in an optional, or an empty optional when the map has none: one search,
by the map's `find` or by the pointer read of a map that gives one, and no exception for an absent key, where
`at` throws. `key` may be of any type the map's `find` takes: a literal or a view for a map keyed by strings,
when the lookup is transparent. On a map with several values per key, the value is the first, the one `find`
finds.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key to look up |

## Return value

An `optional<mapped_type>` holding a copy of the value under `key`, or empty when the map has no value under it.

## Complexity

One search of the map, the complexity of its `find`: constant on average for `map`, `multimap` and `ordered_map`,
logarithmic for `sorted_map` and `sorted_multimap`, logarithmic base 32 for `immutable::map`.

## Exceptions

What the copy constructor of `mapped_type` throws, and what the map's search with a `K` throws: a transparent hash,
equality or comparison called with another type than the key's. None when the copy is noexcept and `K` is the key
type, whose calls the map requires noexcept, or the calls with `K` are noexcept too.

## Notes

The copy keeps nothing of the map: a value changed or erased afterwards does not change it. For the value in
place, without a copy, [try_get](try_get.md).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    sorted_map<string, int> ports = {{"http", 80}, {"https", 443}};
    optional<int> http = ports.get("http");
    println("{} {}", *http, ports.get("ftp").has_value());

    sorted_multimap<int, string> names = {{1, "a"}, {1, "bc"}};
    println("{}", *names.get(1));
}
```

Output:

```text
80 false
a
```

## See also

- [try_get](try_get.md): a pointer to the value, null when absent
- [value_or](value_or.md): the value, or a default
- [sgcl::mixin::lookup\<Derived\>](README.md)
