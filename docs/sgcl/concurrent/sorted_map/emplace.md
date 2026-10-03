[sgcl](../../README.md) › [concurrent](../README.md) › [sorted_map](../sorted_map.md)

# sgcl::concurrent::sorted_map\<Key, T, Compare\>::emplace

```cpp
template<class... A>
pair<iterator, bool> emplace(A&&... a) noexcept(std::is_nothrow_constructible_v<value_type, A...>);
```

Inserts an element constructed from `a...`, unless the map holds its key. The element is built first,
`value_type(std::forward<A>(a)...)` in a new node on the managed heap whose height is drawn at random; then a
search finds the neighbours of its key at every level, and the node is linked into the bottom list with a
compare-exchange, then into its upper levels. When the search finds the key taken, the node is dropped and the
collector reclaims it.

## Parameters

| Parameter | Description |
|---|---|
| `a` | the arguments the element is constructed from: a `value_type`, a key and a value, or `std::piecewise_construct` and two tuples |

## Return value

A pair of an iterator and a `bool`: the inserted element and `true`, or the element already under the key and
`false`, as `std::map::emplace` returns.

## Complexity

Logarithmic in the size of the map, expected, plus a search again after each compare-exchange lost to another
thread. One allocation, made whether the key is taken or not.

## Exceptions

What the constructor of `value_type` throws; none when it is noexcept.

If an exception is thrown, nothing is linked and the map is as it was.

## Notes

Lock-free, and linearizable at the compare-exchange that links the node into the bottom list: of concurrent
insertions of one key, exactly one returns `true`, and the others return its element. The element is built before
its key is known to be absent, the key being inside it: arguments given by rvalue are moved into the node even
when the key is taken. [try_emplace](try_emplace.md) searches first and builds the element only when the key is
absent.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::sorted_map<string, int> scores;
    auto [it, inserted] = scores.emplace("Ada", 3);
    println("{} {} {}", it->first, it->second, inserted);

    atomic<int> winners = 0;
    vector<thread> threads;
    for (int t : range(4)) {
        threads.emplace_back([&scores, &winners, t] {
            winners += scores.emplace("Grace", t).second;  // one thread links its node
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    println("{} winner, {} elements", winners.load(), scores.size());
}
```

Output:

```text
Ada 3 true
1 winner, 2 elements
```

## See also

- [try_emplace](try_emplace.md): builds the element only when the key is absent
- [insert](insert.md): inserts an element or a range
- [sgcl::concurrent::sorted_map\<Key, T, Compare\>](../sorted_map.md)
