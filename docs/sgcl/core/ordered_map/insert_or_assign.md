[sgcl](../../README.md) › [core](../README.md) › [ordered_map](../ordered_map.md)

# sgcl::ordered_map\<Key, T, Hash, KeyEqual\>::insert_or_assign

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

Inserts the element `{key, obj}` when the key is absent, at the end of the order; otherwise assigns `obj` to the
value under the key, in place, and the element keeps its place in the order. The key is looked up once.

1. Copies `key` into a new element.
2. Moves `key` into a new element; a key that is there is not moved from.
3. As (1); `hint` is ignored.
4. As (2); `hint` is ignored.

## Parameters

| Parameter | Description |
|---|---|
| `hint` | an iterator into the map, ignored |
| `key` | the key of the element |
| `obj` | the value to insert or to assign |

## Return value

- (1–2) The element and `true` when it was inserted, `false` when the value was assigned.
- (3–4) An iterator to the element.

## Complexity

Constant on average, linear in `size()` in the worst case; amortized over the growths of the table.

## Exceptions

What the copy or the move of `Key`, the construction of `T` from `obj` or the assignment of `obj` to a `T`
throws; none when they are noexcept.

If an exception is thrown by an insertion, nothing is linked and the map is as it was; by an assignment, the
value is what the assignment of `T` leaves.

## Notes

An assignment does not move the element to the back of the order: a cache that wants it there calls
[to_back](to_back.md) with the iterator returned, as the [example of the class](../ordered_map.md#example) does.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    ordered_map<string, int> m;
    m.insert_or_assign("a", 1);
    m.insert_or_assign("b", 2);

    auto [it, inserted] = m.insert_or_assign("a", 3);
    println("{} {} {}", it->second, inserted, m);

    string key = "c";
    m.insert_or_assign(m.end(), std::move(key), 4);
    println("{}", m);
}
```

Output:

```text
3 false {"a": 3, "b": 2}
{"a": 3, "b": 2, "c": 4}
```

## See also

- [try_emplace](try_emplace.md): inserts only when the key is absent, leaves the value otherwise
- [operator[]](operator_at.md): the value under a key, inserted when absent
- [to_back](to_back.md): moves an element to the end of the order
- [sgcl::ordered_map\<Key, T, Hash, KeyEqual\>](../ordered_map.md)
