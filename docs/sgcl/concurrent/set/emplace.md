[sgcl](../../README.md) › [concurrent](../README.md) › [set](../set.md)

# sgcl::concurrent::set\<Key, Hash, KeyEqual\>::emplace

```cpp
template<class... A>
pair<iterator, bool> emplace(A&&... a) noexcept(std::is_nothrow_constructible_v<value_type, A...>);
```

Inserts a key constructed in place from `a...` unless it is there: `Key(std::forward<A>(a)...)` in a new node on
the managed heap, linked into the list at its split key with a compare-exchange. The key is built first, since its
hash is known only then; when the search finds it there, the node is dropped and the element there stays.

## Parameters

| Parameter | Description |
|---|---|
| `a` | the arguments the key is constructed from |

## Return value

The element and `true`, or the element already equal to the key and `false`.

## Complexity

Constant on average: the search of the key's bucket and one compare-exchange; amortized over the doublings of the
array.

## Exceptions

What the constructor of `Key` throws; none when it is noexcept.

If an exception is thrown, nothing is linked and the set is as it was.

## Notes

Lock-free, and linearizable at the compare-exchange that links the node: of several threads emplacing one key,
exactly one gets `true`. The arguments are consumed whether or not the key is there; [insert](insert.md) of a
built key searches first and builds nothing for a key that is there.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::set<string> lines;

    auto [it, inserted] = lines.emplace(3, '-');  // string(3, '-')
    println("{} {}", *it, inserted);

    println("{}", lines.emplace("---").second);
}
```

Output:

```text
--- true
false
```

## See also

- [insert](insert.md): inserts a built key, searching first
- [sgcl::concurrent::set\<Key, Hash, KeyEqual\>](../set.md)
