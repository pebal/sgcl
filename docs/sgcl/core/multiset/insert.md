[sgcl](../../README.md) › [core](../README.md) › [multiset](../multiset.md)

# sgcl::multiset\<Key, Hash, KeyEqual\>::insert

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

Inserts elements, always, as `std::unordered_multiset::insert` does. A new element whose key is there already
goes in front of the elements with that key, which stay adjacent.

1. Inserts a copy of `value`.
2. Inserts `value`, moved.
3. Inserts an element constructed from `value`, through [emplace](emplace.md).
4. As (1); the hint is ignored.
5. As (2); the hint is ignored.
6. As (3); the hint is ignored.
7. Inserts the elements of the range `[first, last)`, one after another.
8. Inserts the elements of `ilist`.
9. Links the node `nh` holds, without copying or moving the element; `nh` is empty afterwards. An empty handle
   inserts nothing.
10. As (9); the hint is ignored.

- (1–10) The table grows before the node is linked when the size has reached `bucket_count() *
  max_load_factor()`: to a power of two at least twice the bucket count, and to 8 buckets at the least.

## Parameters

| Parameter | Description |
|---|---|
| `hint` | an iterator, ignored |
| `value` | the element to insert |
| `first`, `last` | the range of the elements to insert |
| `ilist` | the list of the elements to insert |
| `nh` | a node handle from [extract](extract.md) of a multiset or a [set](../set.md) |

## Return value

- (1–6) The inserted element.
- (7–8) None.
- (9–10) The inserted element, or `end()` for an empty `nh`.

## Complexity

- (1–6), (9–10) Constant on average, the walk of one bucket; a growth relinks every node, amortized constant.
- (7–8) Linear in the number of elements on average.

## Exceptions

- (1–6) What the construction of the element (the copy or the move of `Key`, its construction from `value`)
  throws; none when it is noexcept.
- (7–8) What the construction of an element from `*first` (the copy of an element of `ilist`) throws.
- (9–10) None.

If an exception is thrown, nothing is linked and the multiset is as it was; (7–8) keep the elements inserted
before it.

## Notes

A node always finds its place, so (9) returns the iterator alone, as `std::unordered_multiset`'s does; a
multiset has no `insert_return_type`. The node-handle forms move an element between multisets, or out of a set
and back, with no copy of the element.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    multiset<string> s;
    s.insert("a");
    auto it = s.insert("a");
    println("{} {}", s.find("a") == it, s.count("a"));  // the new one first

    s.insert(s.end(), "z");  // the hint is ignored
    s.insert({"b", "b"});
    vector<string> more = {"c", "a"};
    s.insert(more.begin(), more.end());
    println("{} {}", s.size(), s.count("a"));

    multiset<string> other = {"q"};
    auto moved = s.insert(other.extract("q"));  // the node is relinked, the string not copied
    println("{} {} {}", *moved, s.size(), other.empty());
}
```

Output:

```text
true 2
7 3
q 8 true
```

## See also

- [emplace](emplace.md): constructs the element in place
- [extract](extract.md): takes a node out of a multiset
- [merge](merge.md): relinks every node of another multiset
- [sgcl::multiset\<Key, Hash, KeyEqual\>](../multiset.md)
