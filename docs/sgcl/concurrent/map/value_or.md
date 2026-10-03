[sgcl](../../README.md) › [concurrent](../README.md) › [map](../map.md)

# sgcl::concurrent::map\<Key, T, Hash, KeyEqual\>::value_or

```cpp
T value_or(const Key& key, const T& fallback) const       // (1)
    noexcept(std::is_nothrow_copy_constructible_v<T>);
template<class K>
T value_or(const K& key, const T& fallback) const         // (2)
    noexcept(std::is_nothrow_copy_constructible_v<T>);
```

Returns a copy of the value under `key`, or `fallback` when the key is not there: the search of [find](find.md),
then a copy of what it found. It is the `value_or` of the library's other maps
([mixin::lookup](../../core/mixin/lookup.md)).

1. The key is of the key type.
2. The key is of any type the hash and the equality take. Takes part only when `Hash` and `KeyEqual` both declare
   `is_transparent`.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key of the value |
| `fallback` | the value returned when the key is not there |

## Return value

A copy of the value under `key`, or a copy of `fallback`.

## Complexity

Constant on average, as [find](find.md), plus the copy of the value.

## Exceptions

What the copy of `T` throws; none when it is noexcept.

## Notes

Wait-free once the key's bucket has its dummy node, as `find`, and then the copy. An element another thread
erases after the search found it is read as it was. A `tracked_ptr` value is one word to copy, and the object it
holds stays alive as long as the copy, whatever the map does with its element.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Session {
    string user;
};

int main() {
    concurrent::map<string, int> ports = {{"http", 80}, {"https", 443}};
    println("{} {}", ports.value_or("https", 0), ports.value_or("gopher", 70));

    concurrent::map<int, tracked_ptr<Session>> sessions;
    sessions.try_emplace(7, make_tracked<Session>("ada"));

    tracked_ptr session = sessions.value_or(7, nullptr);
    sessions.erase(7);
    println("{} {}", session->user, sessions.value_or(7, nullptr) == nullptr);
}
```

Output:

```text
443 70
ada true
```

## See also

- [find](find.md): an iterator to the element
- [contains](contains.md): checks whether a key is there
- [sgcl::concurrent::map\<Key, T, Hash, KeyEqual\>](../map.md)
