[sgcl](../../README.md) › [concurrent](../README.md) › [set](README.md)

# sgcl::concurrent::set\<Key, Hash, KeyEqual\>::key_eq

```cpp
key_equal key_eq() const noexcept(std::is_nothrow_copy_constructible_v<key_equal>);
```

Returns a copy of the function object the set compares its keys with.

## Parameters

None.

## Return value

A copy of the equality of the keys.

## Complexity

Constant.

## Exceptions

What the copy of `KeyEqual` throws; none when it is noexcept.

## Notes

The set never changes its equality, so the call may run concurrently with anything. Two keys the equality takes
for one must have one hash: a search compares the key only with the elements at its hash's place in the list.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct NoCase {
    bool operator()(const string& a, const string& b) const noexcept {
        return a.to_lower() == b.to_lower();
    }
};

struct NoCaseHash {
    size_t operator()(const string& s) const noexcept {
        return s.to_lower().hash();
    }
};

int main() {
    concurrent::set<string, NoCaseHash, NoCase> users;
    users.insert("Ada");

    println("{}", users.key_eq()("ADA", "ada"));
    println("{} {}", users.insert("aDa").second, users.size());
}
```

Output:

```text
true
false 1
```

## See also

- [hash_function](hash_function.md): the hash function
- [find](find.md): the search that compares the keys
- [sgcl::concurrent::set\<Key, Hash, KeyEqual\>](README.md)
