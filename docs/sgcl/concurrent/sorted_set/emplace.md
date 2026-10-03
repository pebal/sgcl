[sgcl](../../README.md) › [concurrent](../README.md) › [sorted_set](README.md)

# sgcl::concurrent::sorted_set\<Key, Compare\>::emplace

```cpp
template<class... A>
pair<iterator, bool> emplace(A&&... a) noexcept(std::is_nothrow_constructible_v<value_type, A...>);
```

Inserts a key constructed from `a...`, unless the set holds it. The key is built first,
`Key(std::forward<A>(a)...)` in a new node on the managed heap whose height is drawn at random; then a search finds
its neighbours at every level, and the node is linked into the bottom list with a compare-exchange, then into its
upper levels. When the search finds the key there, the node is dropped and the collector reclaims it.

## Parameters

| Parameter | Description |
|---|---|
| `a` | the arguments the key is constructed from |

## Return value

A pair of an iterator and a `bool`: the inserted key and `true`, or the key already in the set and `false`.

## Complexity

Logarithmic in the size of the set, expected, plus a search again after each compare-exchange lost to another
thread. One allocation, made whether the key is there or not.

## Exceptions

What the constructor of `Key` throws; none when it is noexcept.

If an exception is thrown, nothing is linked and the set is as it was.

## Notes

Lock-free, and linearizable at the compare-exchange that links the node into the bottom list: of concurrent
insertions of one key, exactly one returns `true`. The key is built before it is known to be absent: arguments
given by rvalue are moved into the node even when the set holds the key. [insert](insert.md) of a key searches
first and builds the node only when the key is absent.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::sorted_set<string> words;
    auto [it, inserted] = words.emplace(3, 'z');  // string(3, 'z')
    println("{} {}", *it, inserted);
    println("{}", words.emplace("zzz").second);
}
```

Output:

```text
zzz true
false
```

## See also

- [insert](insert.md): inserts a key with one search, or a range
- [sgcl::concurrent::sorted_set\<Key, Compare\>](README.md)
