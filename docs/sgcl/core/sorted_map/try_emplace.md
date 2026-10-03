[sgcl](../../README.md) › [core](../README.md) › [sorted_map](README.md)

# sgcl::sorted_map\<Key, T, Compare\>::try_emplace

```cpp
template<class... A>
pair<iterator, bool> try_emplace(const key_type& key, A&&... a)             // (1)
    noexcept(std::is_nothrow_copy_constructible_v<Key> &&
             std::is_nothrow_constructible_v<T, A&&...>);
template<class... A>
pair<iterator, bool> try_emplace(key_type&& key, A&&... a)                  // (2)
    noexcept(std::is_nothrow_move_constructible_v<Key> &&
             std::is_nothrow_constructible_v<T, A&&...>);
template<class... A>
iterator try_emplace(const_iterator hint, const key_type& key, A&&... a)    // (3)
    noexcept(std::is_nothrow_copy_constructible_v<Key> &&
             std::is_nothrow_constructible_v<T, A&&...>);
template<class... A>
iterator try_emplace(const_iterator hint, key_type&& key, A&&... a)         // (4)
    noexcept(std::is_nothrow_move_constructible_v<Key> &&
             std::is_nothrow_constructible_v<T, A&&...>);
```

Looks `key` up first and, only when the map does not hold it, inserts an element whose key is made from `key` and
whose value is built in place from `a...`, as `T(std::forward<A>(a)...)`. The mapped type need not be movable or
copyable, and `a...` are not touched when the key is there.

1. The key is copied into the node.
2. The key is moved into the node; not moved from when the key is there.
3. As (1), with `hint` the element the new one should go right before.
4. As (2), with `hint`.

## Parameters

| Parameter | Description |
|---|---|
| `hint` | the element before which the new one should go; `end()` for the end |
| `key` | the key of the element |
| `a` | the arguments the mapped value is constructed from |

## Return value

- (1–2) A pair of an iterator and a `bool`: the inserted element and `true`, or the element already under the key
  and `false`.
- (3–4) An iterator to the inserted element, or to the one already under the key.

## Complexity

- (1–2) Logarithmic in the size of the map.
- (3–4) Amortized constant when the key belongs right before `hint`, otherwise logarithmic.

## Exceptions

What the copy (1, 3) or the move (2, 4) of `Key` and the constructor of `T` throw; none when they are noexcept.

If an exception is thrown, nothing is inserted.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <memory>

using namespace sgcl;

struct Pinned {
    int value;
    explicit Pinned(int v) : value(v) {}
    Pinned(const Pinned&) = delete;
    Pinned& operator=(const Pinned&) = delete;
};

int main() {
    sorted_map<int, Pinned> m;
    m.try_emplace(1, 10);  // Pinned(10) built inside the node
    auto [it, inserted] = m.try_emplace(1, 11);  // no Pinned(11) is built
    println("{} {}", it->second.value, inserted);

    auto last = m.try_emplace(m.end(), 2, 20);
    println("{} {}", last->second.value, m.size());

    sorted_map<string, std::unique_ptr<int>> owners;
    auto owned = std::make_unique<int>(1);
    owners.try_emplace("a", std::move(owned));
    owners.try_emplace("a", std::move(owned));  // "a" is taken: owned is left as it was
    println("{} {}", *owners.at("a"), owned == nullptr);
}
```

Output:

```text
10 false
20 2
1 true
```

## See also

- [emplace](emplace.md): builds the element before the search
- [insert_or_assign](insert_or_assign.md): inserts an element or assigns to its value
- [operator[]](operator_at.md): the value under a key, inserted when absent
- [sgcl::sorted_map\<Key, T, Compare\>](README.md)
