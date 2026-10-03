[sgcl](../../README.md) › [core](../README.md) › [map](../map.md)

# sgcl::map\<Key, T, Hash, KeyEqual\>::insert

```cpp
pair<iterator, bool> insert(const value_type& value)                   // (1)
    noexcept(std::is_nothrow_copy_constructible_v<value_type>);
pair<iterator, bool> insert(value_type&& value)                        // (2)
    noexcept(std::is_nothrow_move_constructible_v<value_type>);
template<class P> requires std::is_constructible_v<value_type, P&&>
pair<iterator, bool> insert(P&& value)                                 // (3)
    noexcept(std::is_nothrow_constructible_v<value_type, P&&>);
iterator insert(const_iterator hint, const value_type& value)          // (4)
    noexcept(std::is_nothrow_copy_constructible_v<value_type>);
iterator insert(const_iterator hint, value_type&& value)               // (5)
    noexcept(std::is_nothrow_move_constructible_v<value_type>);
template<class P> requires std::is_constructible_v<value_type, P&&>
iterator insert(const_iterator hint, P&& value)                        // (6)
    noexcept(std::is_nothrow_constructible_v<value_type, P&&>);
template<std::input_iterator InputIt>
void insert(InputIt first, InputIt last);                              // (7)
void insert(std::initializer_list<value_type> ilist);                  // (8)
insert_return_type insert(node_type&& nh) noexcept;                    // (9)
iterator insert(const_iterator hint, node_type&& nh) noexcept;         // (10)
```

Inserts elements, or a node, under keys that are not in the map yet, as `std::unordered_map::insert` does.

1. Inserts a copy of `value`.
2. Inserts `value`, moved.
3. Inserts an element constructed from `value`: a `pair` of other types than `value_type`'s.
4. As (1); the hint is ignored.
5. As (2); the hint is ignored.
6. As (3); the hint is ignored.
7. Inserts the elements of the range `[first, last)`, one after another.
8. Inserts the elements of `ilist`.
9. Inserts the node `nh` holds, without copying or moving its element.
10. As (9); the hint is ignored.

- (1–2), (4–5), (7–8) The key is looked up first: when it is there, nothing is built and `value` is left as it
  was. An element of the range (7) that is a `std::pair<Key, U>` is hashed and looked up where it is and copied
  once, into its node, and a duplicate copies nothing; an element of any other type is converted to a
  `value_type` first, once.
- (3), (6) The element is built in a new node first, as by [emplace](emplace.md), and destroyed again when its
  key is there.
- (9–10) On success `nh` is empty afterwards. When the key is there, (9) gives the node back in the result's
  `node`, and (10) leaves it in `nh`, as it was. An empty `nh` inserts nothing.

The table grows before a new node is linked when the size has reached `bucket_count() * max_load_factor()`; an
insertion under a key that is there never makes it grow.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the element to insert |
| `hint` | an iterator into the map, not used |
| `first`, `last` | the range of the elements to insert |
| `ilist` | the list of the elements to insert |
| `nh` | the node handle whose node to insert |

## Return value

- (1–3) An iterator to the element under the key and `true` when it was inserted, or to the element already
  there and `false`.
- (4–6) An iterator to the element under the key, inserted or already there.
- (7–8) None.
- (9) `{position, inserted, node}`: when `nh` is empty, `{end(), false, empty}`; when the node was inserted, an
  iterator to its element, `true` and an empty handle; otherwise an iterator to the element already under the
  key, `false` and the node of `nh`.
- (10) An iterator to the element under the key, inserted or already there; `end()` when `nh` is empty.

## Complexity

- (1–6), (9–10) Constant on average, linear in the size when every key falls into one bucket; a growth relinks
  every node, amortized constant.
- (7–8) Linear in the number of elements inserted, on average.

## Exceptions

- (1–6) What the construction of the element (the copy or the move of `value_type`, its construction from
  `value`) throws; none when it is noexcept.
- (7–8) What the construction of an element from `*first` (the copy of an element of `ilist`) throws.
- (9–10) None.

If an exception is thrown, the element that threw is not inserted and the map is as it was; (7–8) keep the
elements inserted before it.

## Notes

No insertion invalidates an iterator: a growth relinks the nodes, it moves no element.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    map<string, int> m;
    auto [it, inserted] = m.insert({"a", 1});
    println("{} {} {}", it->first, it->second, inserted);

    inserted = m.insert(pair<const char*, int>("a", 2)).second;  // the key is taken
    println("{} {}", inserted, m.at("a"));

    m.insert({{"b", 2}, {"c", 3}});
    vector<pair<string, int>> more = {{"d", 4}, {"b", 20}};
    m.insert(more.begin(), more.end());
    println("{} {}", m.size(), m.at("b"));

    map<string, int> other = {{"q", 17}, {"a", 99}};
    auto r = m.insert(other.extract("q"));  // relinked, nothing copied
    println("{} {} {}", r.inserted, r.position->second, r.node.empty());
    r = m.insert(other.extract("a"));  // the key is taken: the node comes back
    println("{} {} {}", r.inserted, r.position->second, r.node.mapped());
}
```

Output:

```text
a 1 true
false 1
4 2
true 17 true
false 1 99
```

## See also

- [emplace](emplace.md): constructs the element in place
- [try_emplace](try_emplace.md): builds the element only when the key is absent
- [insert_or_assign](insert_or_assign.md): inserts, or assigns to the value under the key
- [extract](extract.md): unlinks an element into a node handle
- [sgcl::map\<Key, T, Hash, KeyEqual\>](../map.md)
