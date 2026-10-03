[sgcl](../../README.md) › [concurrent](../README.md) › [weak_map](README.md)

# sgcl::concurrent::weak_map\<Key, T\>::try_emplace

```cpp
template<class... A>
pair<iterator, bool> try_emplace(const key_pointer& object, A&&... a)
    noexcept(std::is_nothrow_constructible_v<T, A...>);
```

Inserts a value for `object`, constructed in place as `T(std::forward<A>(a)...)`, unless the object has an entry.
The table is searched once: when the object has an entry, nothing is built, neither the value nor the weak pointer,
and `a` is left untouched; when it has none, the node is made with the object's weak pointer, the hash of its
address and the value, and linked where the search found its place with a compare-exchange. When another thread
links an entry for the same object first, the search is repeated from there and finds it.

`object` may not be null: a null pointer is not an object, and debug builds assert.

## Parameters

| Parameter | Description |
|---|---|
| `object` | the object to attach the value to |
| `a` | the arguments the value is constructed from |

## Return value

A pair of an iterator to the entry of `object`, holding the node and the object, and `true` when this call inserted
it; or the iterator to the entry already there and `false`.

## Complexity

Constant on average, plus, when the insertion brings the count since the last sweep to the threshold, a
[sweep](sweep.md), linear in the number of entries: amortized constant, as the threshold is the number of entries
the map had after the last sweep, 16 at least.

## Exceptions

What the constructor of `T` throws; none when it is noexcept.

When the constructor of `T` throws, nothing is linked and the map is as it was.

## Notes

Lock-free, and linearizable at the compare-exchange that links the node: of two threads inserting the same object,
exactly one gets `true`, and the other the entry the first made. A value built for a link that another thread's
insertion of the same object then beat is never in the map; it is left to the collector with its node.

Every insertion counts towards the next sweep. The thread whose insertion brings the count since the last sweep to
the threshold runs the sweep itself, before `try_emplace` returns, unless another thread's sweep is under way; then
it goes on without waiting, so an insertion never waits for a sweep.

There is no `operator[]` and no `insert_or_assign`: a value is set once, at the insertion. A value that changes is
an [atomic](../../core/atomic.md), or holds one, and is changed through the iterator the insertion returns.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Node {
    int id;
};

int main() {
    concurrent::weak_map<Node, string> names;
    tracked_ptr node = make_tracked<Node>(1);
    auto [it, added] = names.try_emplace(node, 3, 'a');  // string(3, 'a')
    println("{} {}", added, it->value);

    auto [again, added_again] = names.try_emplace(node, "other");
    println("{} {}", added_again, again->value);

    tracked_ptr shared = make_tracked<Node>(2);
    atomic<int> winners = 0;
    vector<thread> threads;
    for (int t : range(4)) {
        threads.emplace_back([&, t] {
            winners += names.try_emplace(shared, "thread " + to_string(t)).second;
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    println("{} winner, {} entries", winners.load(), names.size());
}
```

Output:

```text
true aaa
false aaa
1 winner, 2 entries
```

## See also

- [emplace](emplace.md), [insert](insert.md): the same insertion under the names of `std`
- [find](find.md): the entry of an object
- [sgcl::concurrent::weak_map\<Key, T\>](README.md)
