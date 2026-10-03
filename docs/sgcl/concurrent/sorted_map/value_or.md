[sgcl](../../README.md) › [concurrent](../README.md) › [sorted_map](../sorted_map.md)

# sgcl::concurrent::sorted_map\<Key, T, Compare\>::value_or

```cpp
T value_or(const Key& key, const T& fallback) const       // (1)
    noexcept(std::is_nothrow_copy_constructible_v<T>);
template<class K>
T value_or(const K& key, const T& fallback) const         // (2)
    noexcept(std::is_nothrow_copy_constructible_v<T>);
```

Returns a copy of the mapped value under `key`, or a copy of `fallback` when the map holds no element under it:
[find](find.md) and a copy, in one call, as the `value_or` of the other maps of the library
([mixin::lookup](../../core/mixin/lookup.md)).

- (2) Takes part only when `Compare` declares `is_transparent`: the key is of any type the comparison takes, and
  no `Key` is built for the search.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key of the element to read |
| `fallback` | the value returned when the key is absent |

## Return value

A copy of the mapped value under `key`, or of `fallback`.

## Complexity

Logarithmic in the size of the map, expected, plus the copy of `T`.

## Exceptions

What the copy of `T` throws; none when it is noexcept.

## Notes

The search is that of `find`, wait-free and writing nothing. An element erased by another thread after the search
found it is read as it was: the search's iterator holds its node until the copy is made. The copy of a
`tracked_ptr` value is one word. A mapped value that another thread writes through an iterator at the same time
is a race the program synchronizes.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::sorted_map<string, int> ports = {{"http", 80}, {"https", 443}};
    println("{} {}", ports.value_or("https", 0), ports.value_or("ftp", 21));
}
```

Output:

```text
443 21
```

## See also

- [find](find.md): an iterator to the element, which holds it
- [contains](contains.md): checks whether the map holds a key
- [sgcl::concurrent::sorted_map\<Key, T, Compare\>](../sorted_map.md)
