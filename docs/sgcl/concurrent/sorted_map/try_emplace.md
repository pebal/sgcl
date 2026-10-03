[sgcl](../../README.md) › [concurrent](../README.md) › [sorted_map](../sorted_map.md)

# sgcl::concurrent::sorted_map\<Key, T, Compare\>::try_emplace

```cpp
/*(1)*/ template<class... A>
        pair<iterator, bool> try_emplace(const Key& key, A&&... a)
            noexcept(std::is_nothrow_copy_constructible_v<Key> &&
                     std::is_nothrow_constructible_v<T, A...>);
/*(2)*/ template<class... A>
        pair<iterator, bool> try_emplace(Key&& key, A&&... a)
            noexcept(std::is_nothrow_move_constructible_v<Key> &&
                     std::is_nothrow_constructible_v<T, A...>);
```

Inserts an element under `key` with the mapped value constructed from `a...`, unless the map holds the key. One
search finds the key's neighbours at every level; when the key is there, nothing is built. When it is absent, the
element is built, the key from `key` and the value as `T(std::forward<A>(a)...)`, in a node whose height is drawn
against the levels the search saw, and linked between the neighbours that search found: into the bottom list with
a compare-exchange, then into its upper levels. A neighbour that changed meanwhile fails the exchange, and the
search is done again, as after any lost exchange.

1. The key is copied into the node.
2. The key is moved into the node.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key of the element |
| `a` | the arguments the mapped value is constructed from |

## Return value

A pair of an iterator and a `bool`: the inserted element and `true`, or the element already under the key and
`false`.

## Complexity

Logarithmic in the size of the map, expected, plus a search again after each compare-exchange lost to another
thread. One search for a key that may be present, where a lookup and then [emplace](emplace.md) would be two;
no allocation when the key is there.

## Exceptions

What the copy or the move of `Key` and the constructor of `T` throw; none when they are noexcept.

If an exception is thrown, nothing is linked and the map is as it was.

## Notes

Lock-free, and linearizable at the compare-exchange that links the node into the bottom list: of concurrent
insertions of one key, exactly one returns `true`, and the others return its element. This is Java's
`putIfAbsent`.

A key given by rvalue (2) is looked up first and moved only into a node of its own. When another thread inserts
the same key between that search and the link, the search done again finds it, and the key and the arguments
have been moved into a node that is dropped: the one case where the standard's promise of no move for a key
already there does not hold.

The single search took an insertion on one thread from 443 to 214 ns
([Benchmarks: Concurrent containers](../benchmarks.md#concurrent-containers)).

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Session {
    int user, thread;
};

int main() {
    concurrent::sorted_map<int, Session> sessions;
    atomic<int> created = 0;
    vector<thread> threads;
    for (int t : range(4)) {
        threads.emplace_back([&sessions, &created, t] {
            for (int user : range(100)) {
                auto [it, inserted] = sessions.try_emplace(user, user, t);  // Session(user, t)
                created += inserted;
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    println("{} created, {} sessions", created.load(), sessions.size());

    auto [it, inserted] = sessions.try_emplace(42, 42, 9);  // the key is there: nothing built
    println("{} {}", it->second.user, inserted);
}
```

Output:

```text
100 created, 100 sessions
42 false
```

## See also

- [emplace](emplace.md): builds the element before the search
- [insert](insert.md): inserts an element or a range
- [value_or](value_or.md): reads a value without inserting
- [sgcl::concurrent::sorted_map\<Key, T, Compare\>](../sorted_map.md)
