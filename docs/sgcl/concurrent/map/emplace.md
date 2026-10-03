[sgcl](../../README.md) › [concurrent](../README.md) › [map](README.md)

# sgcl::concurrent::map\<Key, T, Hash, KeyEqual\>::emplace

```cpp
template<class... A>
pair<iterator, bool> emplace(A&&... a) noexcept(std::is_nothrow_constructible_v<value_type, A...>);
```

Inserts an element constructed in place from `a...` unless its key is taken: `value_type(std::forward<A>(a)...)`
in a new node on the managed heap, linked into the list at its split key with a compare-exchange. The element is
built first, since its key is known only then; when the search finds the key taken, the node is dropped and the
element there stays.

## Parameters

| Parameter | Description |
|---|---|
| `a` | the arguments the element is constructed from: a key and a value, a `pair`, or `std::piecewise_construct` and two tuples |

## Return value

The element and `true`, or the element already under the key and `false`.

## Complexity

Constant on average: the search of the key's bucket and one compare-exchange; amortized over the doublings of the
array.

## Exceptions

What the constructor of the element throws; none when it is noexcept.

If an exception is thrown, nothing is linked and the map is as it was.

## Notes

Lock-free, and linearizable at the compare-exchange that links the node: of several threads emplacing one key,
exactly one gets `true`. The arguments are consumed whether or not the key is there; when the element is costly to
build, or its arguments are moved from, [try_emplace](try_emplace.md) searches first and builds nothing for a key
that is there.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::map<int, string> names;

    auto [it, inserted] = names.emplace(1, "Ada");
    println("{} {} {}", it->first, it->second, inserted);

    auto [kept, again] = names.emplace(1, "Grace");
    println("{} {}", kept->second, again);
}
```

Output:

```text
1 Ada true
Ada false
```

## See also

- [try_emplace](try_emplace.md): builds the element only when the key is absent
- [insert](insert.md): inserts a built element
- [sgcl::concurrent::map\<Key, T, Hash, KeyEqual\>](README.md)
