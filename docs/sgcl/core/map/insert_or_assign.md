[sgcl](../../README.md) › [core](../README.md) › [map](../map.md)

# sgcl::map\<Key, T, Hash, KeyEqual\>::insert_or_assign

```cpp
/*(1)*/ template<class M>
        pair<iterator, bool> insert_or_assign(const key_type& key, M&& obj)
            noexcept(std::is_nothrow_copy_constructible_v<Key> &&
                     std::is_nothrow_constructible_v<T, M&&> &&
                     std::is_nothrow_assignable_v<T&, M&&>);
/*(2)*/ template<class M>
        pair<iterator, bool> insert_or_assign(key_type&& key, M&& obj)
            noexcept(std::is_nothrow_move_constructible_v<Key> &&
                     std::is_nothrow_constructible_v<T, M&&> &&
                     std::is_nothrow_assignable_v<T&, M&&>);
/*(3)*/ template<class M>
        iterator insert_or_assign(const_iterator hint, const key_type& key, M&& obj)
            noexcept(std::is_nothrow_copy_constructible_v<Key> &&
                     std::is_nothrow_constructible_v<T, M&&> &&
                     std::is_nothrow_assignable_v<T&, M&&>);
/*(4)*/ template<class M>
        iterator insert_or_assign(const_iterator hint, key_type&& key, M&& obj)
            noexcept(std::is_nothrow_move_constructible_v<Key> &&
                     std::is_nothrow_constructible_v<T, M&&> &&
                     std::is_nothrow_assignable_v<T&, M&&>);
```

Inserts `{key, obj}` when the key is not in the map, otherwise assigns `obj` to the value under it, in place.

1. The key is copied into the new element.
2. The key is moved into the new element.
3. As (1); the hint is ignored.
4. As (2); the hint is ignored.

The key is looked up first: the element is built only when it is absent, the value constructed from `obj` in the
node; when it is there, `obj` is assigned to the value and the key is not touched.

## Parameters

| Parameter | Description |
|---|---|
| `hint` | an iterator into the map, not used |
| `key` | the key of the element |
| `obj` | the value to insert or assign |

## Return value

- (1–2) An iterator to the element under the key and `true` when it was inserted, `false` when the value was
  assigned.
- (3–4) An iterator to the element under the key.

## Complexity

Constant on average, linear in the size when every key falls into one bucket; a growth relinks every node,
amortized constant.

## Exceptions

What the construction of the key (its copy or its move) and of the value from `obj`, or the assignment of `obj` to
the value, throws; none when they are noexcept.

If the construction throws, nothing is inserted; if the assignment throws, the value is what the assignment of `T`
leaves.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <tuple>

using namespace sgcl;

int main() {
    map<string, int> m;
    auto [it, inserted] = m.insert_or_assign("a", 1);
    println("{} {} {}", it->first, it->second, inserted);

    std::tie(it, inserted) = m.insert_or_assign("a", 2);
    println("{} {} {}", it->first, it->second, inserted);

    auto last = m.insert_or_assign(m.end(), "z", 26);
    println("{} {}", last->second, m.size());
}
```

Output:

```text
a 1 true
a 2 false
26 2
```

## See also

- [try_emplace](try_emplace.md): builds the element only when the key is absent, never assigns
- [operator[]](operator_at.md): the value under a key, inserted when absent
- [sgcl::map\<Key, T, Hash, KeyEqual\>](../map.md)
