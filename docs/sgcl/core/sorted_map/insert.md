[sgcl](../../README.md) › [core](../README.md) › [sorted_map](../sorted_map.md)

# sgcl::sorted_map\<Key, T, Compare\>::insert

```cpp
/*(1)*/ pair<iterator, bool> insert(const value_type& value)
            noexcept(std::is_nothrow_copy_constructible_v<value_type>);
/*(2)*/ template<class P> requires std::is_constructible_v<value_type, P&&>
        pair<iterator, bool> insert(P&& value)
            noexcept(std::is_nothrow_constructible_v<value_type, P&&>);
/*(3)*/ pair<iterator, bool> insert(value_type&& value)
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
/*(9)*/ insert_return_type insert(node_type&& nh) noexcept;
/*(10)*/ iterator insert(const_iterator hint, node_type&& nh) noexcept;
```

Inserts elements whose keys the map does not hold, as `std::map::insert` does.

1. Inserts a copy of `value`.
2. Inserts an element constructed from `value`, a pair of other types (`std::pair<const char*, int>` for a map of
   `string` to `int`), through [emplace](emplace.md): the element is built in a new node first, and destroyed
   again when the key is there.
3. Inserts `value`, moved.
4. As (1), with `hint` the element the new one should go right before.
5. As (2), with `hint`.
6. As (3), with `hint`.
7. Inserts the elements of the range `[first, last)`, one by one with `end()` as the hint. A range of another
   type is converted once per element, into the node, as [emplace_hint](emplace_hint.md) would.
8. Inserts the elements of `ilist`, as (7).
9. Links the node that `nh` owns, without copying the element. On success `nh` is empty afterwards; when the key
   is there, the node goes back in the `node` of the result. An empty handle inserts nothing.
10. As (9), with `hint`; when the key is there, `nh` keeps the node.

- (1), (3–4), (6) Nothing is built when the key is there; `value` is left as it was.
- (4–6), (10) The hint is used when the key belongs right before it: the insertion costs one or two comparisons
  instead of a search, and an append in sorted order at `end()` costs one.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the element to insert, or what it is constructed from |
| `hint` | the element before which the new one should go; `end()` for the end |
| `first`, `last` | the range of the elements to insert |
| `ilist` | the list of the elements to insert |
| `nh` | the node handle whose node to insert |

## Return value

- (1–3) A pair of an iterator and a `bool`: the inserted element and `true`, or the element already under the key
  and `false`.
- (4–6) An iterator to the inserted element, or to the one already under the key.
- (7–8) None.
- (9) An `insert_return_type`: `position` the inserted element, or the one in the way; `inserted` whether the
  node was linked; `node` empty, or the node that was not linked. For an empty handle `position` is `end()` and
  `inserted` is `false`.
- (10) An iterator to the inserted element, or to the one already under the key; `end()` for an empty handle.

## Complexity

- (1–3), (9) Logarithmic in the size of the map.
- (4–6), (10) Amortized constant when the key belongs right before `hint`, otherwise logarithmic.
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

using namespace sgcl;

int main() {
    sorted_map<string, int> m;
    auto [it, inserted] = m.insert({"a", 1});
    println("{} {}", *it, inserted);

    inserted = m.insert(std::pair<const char*, int>("a", 2)).second;
    println("{} {}", inserted, m.at("a"));

    m.insert(m.end(), {"z", 26});  // an append: one comparison
    m.insert({{"b", 2}, {"c", 3}});
    println("{}", m);
}
```

Output:

```text
("a", 1) true
false 1
{"a": 1, "b": 2, "c": 3, "z": 26}
```

The node handles:

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    sorted_map<string, string> m = {{"a", "apple"}};
    sorted_map<string, string> other = {{"q", "quince"}, {"a", "apricot"}};

    auto r = m.insert(other.extract("q"));  // relinked: no string is copied
    println("{} {} {}", *r.position, r.inserted, r.node.empty());

    r = m.insert(other.extract("a"));  // "a" is taken: the node comes back
    println("{} {} {}", *r.position, r.inserted, r.node.mapped());
    println("{} {}", m, other.size());
}
```

Output:

```text
("q", "quince") true true
("a", "apple") false apricot
{"a": "apple", "q": "quince"} 0
```

## See also

- [emplace](emplace.md): constructs an element in place
- [try_emplace](try_emplace.md): builds the element only when the key is absent
- [insert_or_assign](insert_or_assign.md): inserts an element or assigns to its value
- [extract](extract.md): takes a node out of a map
- [sgcl::sorted_map\<Key, T, Compare\>](../sorted_map.md)
