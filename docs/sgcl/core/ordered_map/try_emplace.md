[sgcl](../../README.md) › [core](../README.md) › [ordered_map](../ordered_map.md)

# sgcl::ordered_map\<Key, T, Hash, KeyEqual\>::try_emplace

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

Inserts an element under `key` unless the key is there, looking it up first: the element is built only when the
key is absent, its key from `key` and its value in place from `a...` (`std::piecewise_construct`), and goes to
the end of the order. When the key is there, nothing is built, `key` and `a...` are not touched, and the element
keeps its value and its place.

1. Copies `key` into the element.
2. Moves `key` into the element.
3. As (1); `hint` is ignored.
4. As (2); `hint` is ignored.

## Parameters

| Parameter | Description |
|---|---|
| `hint` | an iterator into the map, ignored |
| `key` | the key of the element |
| `a` | the arguments the value is constructed from |

## Return value

- (1–2) The element and `true`, or the element already under the key and `false`.
- (3–4) An iterator to the element inserted, or to the element already under the key.

## Complexity

Constant on average, linear in `size()` in the worst case; amortized over the growths of the table.

## Exceptions

What the copy or the move of `Key` and the constructor of `T` throw; none when they are noexcept.

If an exception is thrown, nothing is linked and the map is as it was.

## Notes

The value is constructed inside the node: `T` need not be copyable or movable. The arguments `a...` are evaluated
by the caller in any case: a `make_tracked` among them allocates whether or not the key is there.

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
    ordered_map<int, Pinned> m;
    m.try_emplace(2, 20);  // Pinned(20) built inside the node
    m.try_emplace(1, 10);

    auto [it, fresh] = m.try_emplace(2, 21);  // no Pinned(21) is built
    println("{} {}", it->second.value, fresh);

    auto last = m.try_emplace(m.begin(), 3, 30);
    println("{} {}", last->first, m.back().first);
}
```

Output:

```text
20 false
3 3
```

## See also

- [emplace](emplace.md): builds the element first
- [insert_or_assign](insert_or_assign.md): assigns to the value when the key is there
- [operator[]](operator_at.md): the value under a key, value-initialized when absent
- [sgcl::ordered_map\<Key, T, Hash, KeyEqual\>](../ordered_map.md)
