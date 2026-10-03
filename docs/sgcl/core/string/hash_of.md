[sgcl](../../README.md) › [core](../README.md) › [string](README.md)

# sgcl::string::hash_of

```cpp
static size_t hash_of(view_type s) noexcept;
```

Returns the hash a string of the characters of `s` has, without making the string: `hash_of(s)` equals
`string(s).hash()`. It is what the transparent `std::hash` of a string computes for a `std::basic_string_view`, a
slice, an array or a pointer, so a `map` or a `set` keyed by strings is searched with any of them and no string is
made for the search: `ages.find("alice")` allocates nothing.

The hash is keyed per process, as [hash](hash.md) is: a value differs from run to run unless `SGCL_HASH_SEED` fixes
the key.

## Parameters

| Parameter | Description |
|---|---|
| `s` | the characters: a view, or anything a view is made of (a literal, a `std::string`, a slice) |

## Return value

The hash a string of these characters has.

## Complexity

Linear in the size of `s`; nothing is kept.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <string_view>

using namespace sgcl;

int main() {
    string name = "alice";
    std::string_view view = "alice";
    std::hash<string> hasher;
    println("{} {}", string::hash_of(view) == name.hash(), hasher("alice") == name.hash());

    map<string, int> ages = {{"alice", 30}, {"bob", 25}};
    auto found = ages.find("alice");  // hashed by hash_of: no string made
    println("{} {}", found->second, ages.contains(view));
}
```

Output:

```text
true true
30 true
```

## See also

- [hash](hash.md): the hash of a string, computed once and kept in the object
- [map](../map/README.md), [set](../set/README.md): the containers a string keys
- [sgcl::string](README.md)
