[sgcl](../../README.md) › [immutable](../README.md) › [map](../map.md)

# sgcl::immutable::map\<Key, T, Hash, KeyEqual\>::hash_function

```cpp
hasher hash_function() const;
```

Returns a copy of the hash the map places its keys by: the one it was constructed with, kept by every version
made from it.

## Parameters

None.

## Return value

A copy of the hash.

## Complexity

Constant.

## Exceptions

What the copy of `Hash` throws.

## Example

```cpp
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

struct by_length {
    size_t operator()(const string& s) const noexcept {
        return s.size();
    }
};

int main() {
    immutable::map<string, int, by_length> words = {{"one", 1}, {"three", 3}};
    auto hash = words.set("four", 4).hash_function();
    println("{} {}", hash("abc"), hash("abcde"));
}
```

Output:

```text
3 5
```

## See also

- [key_eq](key_eq.md): the equality of the keys
- [sgcl::immutable::map\<Key, T, Hash, KeyEqual\>](../map.md)
