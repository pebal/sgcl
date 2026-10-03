[sgcl](../../README.md) › [core](../README.md) › [multimap](../multimap.md)

# sgcl::multimap\<Key, T, Hash, KeyEqual\>::insert

```cpp
/*(1)*/ iterator insert(const value_type& value)
            noexcept(std::is_nothrow_copy_constructible_v<value_type>);
/*(2)*/ iterator insert(value_type&& value)
            noexcept(std::is_nothrow_move_constructible_v<value_type>);
/*(3)*/ template<class P> requires std::is_constructible_v<value_type, P&&>
        iterator insert(P&& value)
            noexcept(std::is_nothrow_constructible_v<value_type, P&&>);
/*(4)*/ iterator insert(const_iterator hint, const value_type& value)
            noexcept(std::is_nothrow_copy_constructible_v<value_type>);
/*(5)*/ iterator insert(const_iterator hint, value_type&& value)
            noexcept(std::is_nothrow_move_constructible_v<value_type>);
/*(6)*/ template<class P> requires std::is_constructible_v<value_type, P&&>
        iterator insert(const_iterator hint, P&& value)
            noexcept(std::is_nothrow_constructible_v<value_type, P&&>);
/*(7)*/ template<std::input_iterator InputIt>
        void insert(InputIt first, InputIt last);
/*(8)*/ void insert(std::initializer_list<value_type> ilist);
/*(9)*/ iterator insert(node_type&& nh) noexcept;
/*(10)*/ iterator insert(const_iterator hint, node_type&& nh) noexcept;
```

Inserts elements, or a node, as `std::unordered_multimap::insert` does: always, whatever keys are there. An
element under a key that is there already goes in front of the elements with that key.

1. Inserts a copy of `value`.
2. Inserts `value`, moved.
3. Inserts an element constructed from `value`, as by [emplace](emplace.md): a `pair` of other types than
   `value_type`'s.
4. As (1); the hint is ignored.
5. As (2); the hint is ignored.
6. As (3); the hint is ignored.
7. Inserts the elements of the range `[first, last)`, one after another.
8. Inserts the elements of `ilist`.
9. Inserts the node `nh` holds, without copying or moving its element; `nh` is empty afterwards. An empty `nh`
   inserts nothing.
10. As (9); the hint is ignored.

- (7) An element of the range that is a `std::pair<Key, U>` is hashed and looked up where it is and copied once,
  into its node; an element of any other type is converted to a `value_type` first, once.

The table grows before a new node is linked when the size has reached `bucket_count() * max_load_factor()`.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the element to insert |
| `hint` | an iterator into the multimap, not used |
| `first`, `last` | the range of the elements to insert |
| `ilist` | the list of the elements to insert |
| `nh` | the node handle whose node to insert |

## Return value

- (1–6) An iterator to the inserted element.
- (7–8) None.
- (9–10) An iterator to the inserted element, or `end()` when `nh` is empty.

## Complexity

- (1–6), (9–10) Constant on average, linear in the size when every key falls into one bucket; a growth relinks
  every node, amortized constant.
- (7–8) Linear in the number of elements inserted, on average.

## Exceptions

- (1–6) What the construction of the element (the copy or the move of `value_type`, its construction from
  `value`) throws; none when it is noexcept.
- (7–8) What the construction of an element from `*first` (the copy of an element of `ilist`) throws.
- (9–10) None.

If an exception is thrown, the element that threw is not inserted and the multimap is as it was; (7–8) keep the
elements inserted before it.

## Notes

No insertion invalidates an iterator: a growth relinks the nodes, it moves no element. A node always finds its
place, so (9) returns the iterator alone, as `std::unordered_multimap`'s does; a multimap has no
`insert_return_type`.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    multimap<string, int> m;
    m.insert({"a", 1});
    auto it = m.insert({"a", 2});  // in front of the first "a"
    println("{} {}", it->second, m.find("a") == it);

    m.insert(pair<const char*, int>("z", 26));
    m.insert({{"b", 2}, {"b", 3}});
    println("{} {}", m.size(), m.count("b"));

    multimap<string, int> other = {{"q", 17}};
    auto moved = m.insert(other.extract("q"));  // relinked, nothing copied
    println("{} {}", moved->second, other.empty());
}
```

Output:

```text
2 true
5 2
17 true
```

## See also

- [emplace](emplace.md): constructs the element in place
- [extract](extract.md): unlinks an element into a node handle
- [equal_range](equal_range.md): the run of the elements under a key
- [sgcl::multimap\<Key, T, Hash, KeyEqual\>](../multimap.md)
