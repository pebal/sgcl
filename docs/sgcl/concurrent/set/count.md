[sgcl](../../README.md) › [concurrent](../README.md) › [set](../set.md)

# sgcl::concurrent::set\<Key, Hash, KeyEqual\>::count

```cpp
/*(1)*/ size_type count(const Key& key) const noexcept;
/*(2)*/ template<class K> size_type count(const K& key) const noexcept;
```

Returns the number of elements equal to `key`, 1 or 0, as `std::unordered_set::count` does: the search of
[find](find.md).

1. The key is of the key type.
2. The key is of any type the hash and the equality take. Takes part only when `Hash` and `KeyEqual` both declare
   `is_transparent`.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key to count |

## Return value

1 when an element is equal to `key`, 0 otherwise.

## Complexity

Constant on average, as [find](find.md).

## Exceptions

None.

## Notes

Wait-free once the key's bucket has its dummy node, as `find`; [contains](contains.md) is the same question with
a `bool` for the answer.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::set<string> stop = {"a", "the", "of"};
    vector<string> words = {"the", "art", "of", "war"};

    size_t stopped = 0;
    for (auto& w : words) {
        stopped += stop.count(w);
    }
    println("{}", stopped);
}
```

Output:

```text
2
```

## See also

- [contains](contains.md): the same answer as a `bool`
- [find](find.md): an iterator to the element
- [sgcl::concurrent::set\<Key, Hash, KeyEqual\>](../set.md)
