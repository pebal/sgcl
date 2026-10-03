[sgcl](../../README.md) › [core](../README.md) › [sorted_map](README.md)

# sgcl::sorted_map\<Key, T, Compare\>::insert_or_assign

```cpp
template<class M>
pair<iterator, bool> insert_or_assign(const key_type& key, M&& obj)             // (1)
    noexcept(std::is_nothrow_copy_constructible_v<Key> &&
             std::is_nothrow_constructible_v<T, M&&> &&
             std::is_nothrow_assignable_v<T&, M&&>);
template<class M>
pair<iterator, bool> insert_or_assign(key_type&& key, M&& obj)                  // (2)
    noexcept(std::is_nothrow_move_constructible_v<Key> &&
             std::is_nothrow_constructible_v<T, M&&> &&
             std::is_nothrow_assignable_v<T&, M&&>);
template<class M>
iterator insert_or_assign(const_iterator hint, const key_type& key, M&& obj)    // (3)
    noexcept(std::is_nothrow_copy_constructible_v<Key> &&
             std::is_nothrow_constructible_v<T, M&&> &&
             std::is_nothrow_assignable_v<T&, M&&>);
template<class M>
iterator insert_or_assign(const_iterator hint, key_type&& key, M&& obj)         // (4)
    noexcept(std::is_nothrow_move_constructible_v<Key> &&
             std::is_nothrow_constructible_v<T, M&&> &&
             std::is_nothrow_assignable_v<T&, M&&>);
```

Looks `key` up once. When the map does not hold it, inserts an element whose key is made from `key` and whose
value is constructed from `obj`; when it does, assigns `obj` to the value under the key, in place. Unlike
[operator[]](operator_at.md), the mapped type need not be default-constructible.

1. The key is copied into the node.
2. The key is moved into the node.
3. As (1), with `hint` the element the new one should go right before.
4. As (2), with `hint`.

## Parameters

| Parameter | Description |
|---|---|
| `hint` | the element before which the new one should go; `end()` for the end |
| `key` | the key of the element |
| `obj` | the value to insert or to assign |

## Return value

- (1–2) A pair of an iterator and a `bool`: the element under the key, and `true` when it was inserted, `false`
  when it was assigned to.
- (3–4) An iterator to the element under the key.

## Complexity

- (1–2) Logarithmic in the size of the map.
- (3–4) Amortized constant when the key belongs right before `hint`, otherwise logarithmic.

## Exceptions

What the copy (1, 3) or the move (2, 4) of `Key`, the construction of `T` from `obj` and the assignment of `obj`
to `T` throw; none when they are noexcept.

If the construction throws, nothing is inserted; if the assignment throws, the value is what the assignment of
`T` left.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    sorted_map<string, int> m;
    auto [it, inserted] = m.insert_or_assign("a", 1);
    println("{} {}", *it, inserted);

    auto again = m.insert_or_assign("a", 2);
    println("{} {}", *again.first, again.second);

    m.insert_or_assign(m.end(), "z", 26);
    println("{}", m);
}
```

Output:

```text
("a", 1) true
("a", 2) false
{"a": 2, "z": 26}
```

## See also

- [try_emplace](try_emplace.md): inserts only when the key is absent, never assigns
- [operator[]](operator_at.md): the value under a key, inserted when absent
- [sgcl::sorted_map\<Key, T, Compare\>](README.md)
