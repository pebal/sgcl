[sgcl](../../README.md) › [immutable](../README.md) › [set](README.md)

# sgcl::immutable::set\<Key, Hash, KeyEqual\>::hash_function

```cpp
hasher hash_function() const;
```

Returns a copy of the hash the set places its elements by: the one it was constructed with, kept by every version
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
    immutable::set<string, by_length> words = {"one", "three"};
    auto hash = words.insert("four").hash_function();
    println("{} {}", hash("abc"), hash("abcde"));
}
```

Output:

```text
3 5
```

## See also

- [key_eq](key_eq.md): the equality of the elements
- [sgcl::immutable::set\<Key, Hash, KeyEqual\>](README.md)
