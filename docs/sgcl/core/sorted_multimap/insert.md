[sgcl](../../README.md) › [core](../README.md) › [sorted_multimap](../sorted_multimap.md)

# sgcl::sorted_multimap\<Key, T, Compare\>::insert

```cpp
/*(1)*/ iterator insert(const value_type& value)
            noexcept(std::is_nothrow_copy_constructible_v<value_type>);
/*(2)*/ template<class P> requires std::is_constructible_v<value_type, P&&>
        iterator insert(P&& value) noexcept(std::is_nothrow_constructible_v<value_type, P&&>);
/*(3)*/ iterator insert(value_type&& value)
            noexcept(std::is_nothrow_move_constructible_v<value_type>);
/*(4)*/ iterator insert(const_iterator hint, const value_type& value)
            noexcept(std::is_nothrow_copy_constructible_v<value_type>);
/*(5)*/ template<class P> requires std::is_constructible_v<value_type, P&&>
        iterator insert(const_iterator hint, P&& value)
            noexcept(std::is_nothrow_constructible_v<value_type, P&&>);
/*(6)*/ iterator insert(const_iterator hint, value_type&& value)
            noexcept(std::is_nothrow_move_constructible_v<value_type>);
/*(7)*/ template<std::input_iterator InputIt>
        void insert(InputIt first, InputIt last);
/*(8)*/ void insert(std::initializer_list<value_type> ilist);
/*(9)*/ iterator insert(node_type&& nh) noexcept;
/*(10)*/ iterator insert(const_iterator hint, node_type&& nh) noexcept;
```

Inserts elements, as `std::multimap::insert` does: always, a key already present getting the new element after
its equivalents.

1. Inserts a copy of `value`.
2. Inserts an element constructed from `value`, a pair of other types (`std::pair<const char*, int>` for a
   multimap of `string` to `int`), through [emplace](emplace.md).
3. Inserts `value`, moved.
4. As (1), with `hint` the element the new one should go right before.
5. As (2), with `hint`.
6. As (3), with `hint`.
7. Inserts the elements of the range `[first, last)`, one by one with `end()` as the hint. A range of another
   type is converted once per element, into the node, as [emplace_hint](emplace_hint.md) would.
8. Inserts the elements of `ilist`, as (7).
9. Links the node that `nh` owns, without copying the element, after its equivalents; `nh` is empty afterwards.
   An empty handle inserts nothing.
10. As (9), with `hint`.

- (4–6), (10) The hint is used when the element belongs right before it, among its equivalents too: the insertion
  costs one or two comparisons instead of a search, and an append in sorted order at `end()` costs one. A hint
  that does not fit is ignored, and the element goes after its equivalents.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the element to insert, or what it is constructed from |
| `hint` | the element before which the new one should go; `end()` for the end |
| `first`, `last` | the range of the elements to insert |
| `ilist` | the list of the elements to insert |
| `nh` | the node handle whose node to insert |

## Return value

- (1–6) An iterator to the inserted element.
- (7–8) None.
- (9–10) An iterator to the inserted element, or `end()` for an empty handle.

## Complexity

- (1–3), (9) Logarithmic in the size of the multimap.
- (4–6), (10) Amortized constant when the element belongs right before `hint`, otherwise logarithmic.
- (7–8) *n* log(*size* + *n*) for *n* elements, linear when they come sorted.

## Exceptions

- (1–6) What the construction of `value_type` (its copy, its move, its construction from `value`) throws; none
  when it is noexcept.
- (7–8) What the construction of `value_type` from an element throws.
- (9–10) None.

If an exception is thrown, the element that threw is not inserted; with (7–8) the elements inserted before it
stay.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <iterator>

using namespace sgcl;

int main() {
    sorted_multimap<string, int> m;
    auto it = m.insert({"a", 1});
    m.insert({"a", 2});  // after the first "a"
    println("{}", std::next(it)->second);

    m.insert(m.end(), std::pair<const char*, int>("z", 26));  // an append: one comparison
    m.insert({{"b", 2}, {"b", 3}});
    println("{}", m);

    sorted_multimap<string, int> other = {{"a", 17}};
    auto moved = m.insert(other.extract("a"));  // relinked, no copy: after the other two
    println("{} {} {}", *moved, m.count("a"), other.empty());
}
```

Output:

```text
2
{"a": 1, "a": 2, "b": 2, "b": 3, "z": 26}
("a", 17) 3 true
```

## See also

- [emplace](emplace.md): constructs an element in place
- [extract](extract.md): takes a node out of a multimap
- [sgcl::sorted_multimap\<Key, T, Compare\>](../sorted_multimap.md)
