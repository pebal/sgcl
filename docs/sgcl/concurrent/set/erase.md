[sgcl](../../README.md) › [concurrent](../README.md) › [set](README.md)

# sgcl::concurrent::set\<Key, Hash, KeyEqual\>::erase

```cpp
size_type erase(const Key& key) noexcept;                    // (1)
template<class K> size_type erase(const K& key) noexcept;    // (2)
iterator erase(const_iterator pos) noexcept;                 // (3)
```

Erases an element. The node is marked first, a marker node linked after it with a compare-exchange, and then
unlinked: with a compare-exchange on its predecessor, or, when the predecessor has moved on, by the next search
that passes it, as every search unlinks the marked nodes it meets.

1. Erases the element equal to `key`, if there is one.
2. The same with a key of any type the hash and the equality take. Takes part only when `Hash` and `KeyEqual`
   both declare `is_transparent`, and `K` is not convertible to `const_iterator`.
3. Erases the element `pos` addresses, if it is still there: another thread may have erased it, and an equal key
   inserted since is another node, which (3) leaves.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key to erase |
| `pos` | an iterator to the element to erase; not `end()` |

## Return value

- (1–2) The number of elements erased, 1 or 0.
- (3) An iterator to the element after `pos` in the list, or [end()](end.md).

## Complexity

Constant on average: the search of the key's bucket, a compare-exchange to mark the node and one to unlink it.

## Exceptions

None.

## Notes

Lock-free, and linearizable at the compare-exchange that marks the node: of several threads erasing one key,
exactly one marks it and gets 1, and the others get 0. The element is not destroyed by the erase: another thread
may be reading it, through an iterator or in the middle of a search, and the collector destroys it with its node
once nothing holds the node. An iterator to the element stays valid and reads it as it was.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::set<int> pending = {1, 2, 3, 4, 5, 6};
    atomic<int> done = 0;
    vector<thread> workers;
    for (int t : range(4)) {
        workers.emplace_back([&] {
            for (int job : range(1, 7)) {
                done += int(pending.erase(job));  // one worker takes each job
            }
        });
    }
    for (auto& w : workers) {
        w.join();
    }
    println("{} {}", done.load(), pending.empty());

    concurrent::set<int> numbers = {1, 2, 3, 4};
    for (auto it = numbers.begin(); it != numbers.end();) {
        if (*it % 2 != 0) {
            it = numbers.erase(it);
        } else {
            ++it;
        }
    }
    println("{} {}", numbers.size(), numbers.contains(2));
}
```

Output:

```text
6 true
2 true
```

## See also

- [clear](clear.md): erases every element
- [insert](insert.md): inserts a key
- [sgcl::concurrent::set\<Key, Hash, KeyEqual\>](README.md)
