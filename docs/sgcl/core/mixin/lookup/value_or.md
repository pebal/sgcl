[sgcl](../../../README.md) › [core](../../README.md) › [mixin](../README.md) › [lookup](README.md)

# sgcl::mixin::lookup\<Derived\>::value_or

```cpp
template<class K, class U>
auto value_or(const K& key, U&& fallback) const noexcept(/* see below */);
```

Returns the value under `key`, copied, or `fallback` converted to `mapped_type` when the map has none: one search,
by the map's `find` or by the pointer read of a map that gives one, and no exception for an absent key. `key` may
be of any type the map's `find` takes. On a map with several values per key, the value is the first, the one
`find` finds.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key to look up |
| `fallback` | the value to return when the map has no value under `key`, anything `mapped_type` is constructed from |

## Return value

A `mapped_type`: a copy of the value under `key`, or `fallback` converted to it.

## Complexity

One search of the map, the complexity of its `find`: constant on average for `map`, `multimap` and `ordered_map`,
logarithmic for `sorted_map` and `sorted_multimap`, logarithmic base 32 for `immutable::map`.

## Exceptions

What the copy constructor of `mapped_type` throws, or its construction from `fallback`, and what the map's search
with a `K` throws: a transparent hash, equality or comparison called with another type than the key's. None when
the copy and the construction are noexcept and `K` is the key type, whose calls the map requires noexcept, or the
calls with `K` are noexcept too.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    sorted_map<string, int> ports = {{"http", 80}, {"https", 443}};
    println("{} {}", ports.value_or("https", 0), ports.value_or("ftp", 21));

    map<int, string> names = {{1, "one"}};
    string name = names.value_or(2, "none");
    println("{}", name);
}
```

Output:

```text
443 21
none
```

## See also

- [get](get.md): a copy of the value in an optional
- [try_get](try_get.md): a pointer to the value, null when absent
- [sgcl::mixin::lookup\<Derived\>](README.md)
