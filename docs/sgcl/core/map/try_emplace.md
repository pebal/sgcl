[sgcl](../../README.md) › [core](../README.md) › [map](../map.md)

# sgcl::map\<Key, T, Hash, KeyEqual\>::try_emplace

```cpp
/*(1)*/ template<class... A>
        pair<iterator, bool> try_emplace(const key_type& key, A&&... a)
            noexcept(std::is_nothrow_copy_constructible_v<Key> &&
                     std::is_nothrow_constructible_v<T, A&&...>);
/*(2)*/ template<class... A>
        pair<iterator, bool> try_emplace(key_type&& key, A&&... a)
            noexcept(std::is_nothrow_move_constructible_v<Key> &&
                     std::is_nothrow_constructible_v<T, A&&...>);
/*(3)*/ template<class... A>
        iterator try_emplace(const_iterator hint, const key_type& key, A&&... a)
            noexcept(std::is_nothrow_copy_constructible_v<Key> &&
                     std::is_nothrow_constructible_v<T, A&&...>);
/*(4)*/ template<class... A>
        iterator try_emplace(const_iterator hint, key_type&& key, A&&... a)
            noexcept(std::is_nothrow_move_constructible_v<Key> &&
                     std::is_nothrow_constructible_v<T, A&&...>);
```

Inserts an element under `key` with the value constructed in place from `a...`, when the key is not in the map.
The key is looked up first: when it is there, nothing is built and `key` and `a...` are not touched.

1. The key is copied into the new element.
2. The key is moved into the new element.
3. As (1); the hint is ignored.
4. As (2); the hint is ignored.

The value is built inside the node, from `a...` as they are: `T` need not be copyable or movable.

## Parameters

| Parameter | Description |
|---|---|
| `hint` | an iterator into the map, not used |
| `key` | the key of the element |
| `a` | the arguments of the value's constructor |

## Return value

- (1–2) An iterator to the element under the key and `true` when it was inserted, or to the element already
  there and `false`.
- (3–4) An iterator to the element under the key, inserted or already there.

## Complexity

Constant on average, linear in the size when every key falls into one bucket; a growth relinks every node,
amortized constant.

## Exceptions

What the construction of the key (its copy or its move) and of the value from `a...` throws; none when they are
noexcept.

If an exception is thrown, nothing is inserted and the map is as it was.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Pinned {
    int value;
    explicit Pinned(int v) : value(v) {}
    Pinned(const Pinned&) = delete;
    Pinned& operator=(const Pinned&) = delete;
};

int main() {
    map<int, Pinned> m;
    m.try_emplace(1, 10);  // Pinned(10) built inside the node
    auto [it, fresh] = m.try_emplace(1, 11);  // the key is taken: no Pinned(11) is built
    println("{} {}", it->second.value, fresh);

    auto hinted = m.try_emplace(m.end(), 2, 20);
    println("{} {}", hinted->second.value, m.size());

    map<string, string> words;
    words.try_emplace("k", 3, 'x');  // string(3, 'x') built in the node
    println("{}", words.at("k"));
}
```

Output:

```text
10 false
20 2
xxx
```

## See also

- [emplace](emplace.md): builds the element before the lookup
- [insert_or_assign](insert_or_assign.md): inserts, or assigns to the value under the key
- [operator[]](operator_at.md): the value under a key, inserted when absent
- [sgcl::map\<Key, T, Hash, KeyEqual\>](../map.md)
