[sgcl](../../README.md) › [concurrent](../README.md) › [map](../map.md)

# sgcl::concurrent::map\<Key, T, Hash, KeyEqual\>::try_emplace

```cpp
template<class... A>
pair<iterator, bool> try_emplace(const Key& key, A&&... a)    // (1)
    noexcept(std::is_nothrow_copy_constructible_v<Key> &&
             std::is_nothrow_constructible_v<T, A...>);
template<class... A>
pair<iterator, bool> try_emplace(Key&& key, A&&... a)         // (2)
    noexcept(std::is_nothrow_move_constructible_v<Key> &&
             std::is_nothrow_constructible_v<T, A...>);
```

Inserts an element under `key` unless the key is taken, searching once: the element is built only when the key is
absent, its key from `key` and its value from `a...` (`std::piecewise_construct`), and linked where that search
found its place. It is Java's `putIfAbsent`, and Go's `LoadOrStore`.

1. Copies `key` into the element.
2. Moves `key` into the element.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key of the element |
| `a` | the arguments the value is constructed from |

## Return value

The element and `true`, or the element already under the key and `false`.

## Complexity

Constant on average: one search of the key's bucket and one compare-exchange; amortized over the doublings of the
array.

## Exceptions

What the copy or the move of `Key` and the constructor of `T` throw; none when they are noexcept.

If an exception is thrown, nothing is linked and the map is as it was.

## Notes

Lock-free, and linearizable at the compare-exchange that links the node: of several threads inserting one key,
exactly one gets `true`, and the others the element it inserted. A key given by rvalue (2) is looked up first and
moved only into a node of its own. When another thread inserts the same key between the search and the link, the
compare-exchange fails, the search runs again and finds that key: the key has been moved into a node that is
dropped, the one case where the standard's promise of no move for a key already there does not hold.

The arguments `a...` are evaluated by the caller in any case: a `make_tracked` among them allocates whether or not
the key is there.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::map<int, int> owners;
    atomic<int> wins = 0;
    vector<thread> threads;
    for (int t : range(8)) {
        threads.emplace_back([&, t] {
            for (int id : range(1000)) {
                wins += owners.try_emplace(id, t).second;  // one thread wins each id
            }
        });
    }
    for (auto& th : threads) {
        th.join();
    }
    println("{} wins, {} ids", wins.load(), owners.size());

    string key = "Ada";
    concurrent::map<string, int> ages;
    ages.try_emplace(std::move(key), 36);
    string again = "Ada";
    auto [it, inserted] = ages.try_emplace(std::move(again), 37);
    println("{} {} '{}'", it->second, inserted, again);  // a key there: again not moved from
}
```

Output:

```text
1000 wins, 1000 ids
36 false 'Ada'
```

## See also

- [emplace](emplace.md): builds the element first
- [insert](insert.md): inserts a built element
- [value_or](value_or.md): a copy of the value under a key
- [sgcl::concurrent::map\<Key, T, Hash, KeyEqual\>](../map.md)
