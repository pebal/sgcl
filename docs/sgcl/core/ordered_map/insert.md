[sgcl](../../README.md) › [core](../README.md) › [ordered_map](../ordered_map.md)

# sgcl::ordered_map\<Key, T, Hash, KeyEqual\>::insert

```cpp
pair<iterator, bool> insert(const value_type& value)                               // (1)
    noexcept(std::is_nothrow_copy_constructible_v<value_type>);
pair<iterator, bool> insert(value_type&& value)                                    // (2)
    noexcept(std::is_nothrow_move_constructible_v<value_type>);
template<class P> requires std::is_constructible_v<value_type, P&&>
pair<iterator, bool> insert(P&& value)                                             // (3)
    noexcept(std::is_nothrow_constructible_v<value_type, P&&>);
iterator insert(const_iterator hint, const value_type& value)                      // (4)
    noexcept(std::is_nothrow_copy_constructible_v<value_type>);
iterator insert(const_iterator hint, value_type&& value)                           // (5)
    noexcept(std::is_nothrow_move_constructible_v<value_type>);
template<class P> requires std::is_constructible_v<value_type, P&&>
iterator insert(const_iterator hint, P&& value)                                    // (6)
    noexcept(std::is_nothrow_constructible_v<value_type, P&&>);
template<std::input_iterator InputIt> void insert(InputIt first, InputIt last);    // (7)
void insert(std::initializer_list<value_type> ilist);                              // (8)
insert_return_type insert(node_type&& nh) noexcept;                                // (9)
iterator insert(const_iterator hint, node_type&& nh) noexcept;                     // (10)
```

Inserts elements unless their keys are there, as `std::unordered_map::insert` does. A new element goes to the
end of the order, the newest; a key that is there keeps its element, its value and its place.

1. Inserts a copy of `value`.
2. Inserts `value`, moved.
3. Inserts an element constructed from `value`: a `pair` of other types than `value_type`'s.
4. As (1); `hint` is ignored.
5. As (2); `hint` is ignored.
6. As (3); `hint` is ignored.
7. Inserts the elements of the range `[first, last)`, in its order.
8. Inserts the elements of `ilist`, in its order.
9. Links the node `nh` holds, the element untouched.
10. As (9); `hint` is ignored.

- (1–2), (4–5) The key is looked up first: when it is there, nothing is built and `value` is left as it was.
- (3), (6) Go through [emplace](emplace.md): the element is built in a new node first, and destroyed again when
  its key is there.
- (7–8) An element that is a `std::pair` with a first of the type `Key` is hashed and looked up where it is and
  copied once, into its node; a duplicate copies nothing. An element of any other type is converted to a
  `value_type` first, once.
- (9–10) No element is copied or moved. On success `nh` is empty after. When the key is there, the node stays in
  a handle: the `node` of the result of (9), `nh` itself for (10). An empty handle inserts nothing. The node goes
  to the end of the order, whatever its place was in the map it came from.

The table grows before a new node is linked when the size has reached `bucket_count() * max_load_factor()`; a key
that is there never grows it.

## Parameters

| Parameter | Description |
|---|---|
| `hint` | an iterator into the map, ignored |
| `value` | the element to insert |
| `first`, `last` | the range of the elements to insert |
| `ilist` | the list of the elements to insert |
| `nh` | a node handle, from [extract](extract.md) of this map or of another `ordered_map` with the same `Key` and `T` |

## Return value

- (1–3) The element and `true`, or the element already under the key and `false`. `pair` is the alias of
  `std::pair` ([aliases](../aliases.md)).
- (4–6) An iterator to the element inserted, or to the element already under the key.
- (7–8) None.
- (9) An `insert_return_type`: `position` the element inserted or the one under the key, `inserted` whether the
  node was linked, `node` the handle when it was not. For an empty `nh`: `end()`, `false` and an empty handle.
- (10) An iterator to the element inserted or to the one under the key, `end()` for an empty `nh`.

## Complexity

- (1–6), (9–10) Constant on average, linear in `size()` in the worst case; amortized over the growths of the
  table.
- (7–8) Linear in the number of elements on average.

## Exceptions

- (1–6) What the construction of the element (the copy or the move of `value_type`, its construction from
  `value`) throws; none when it is noexcept.
- (7–8) What the construction of an element throws.
- (9–10) None.

If an exception is thrown, nothing is linked and the map is as it was; (7–8) keep the elements inserted before
it.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    ordered_map<string, int> m;
    auto [it, inserted] = m.insert({"b", 1});
    println("{} {} {}", it->first, it->second, inserted);

    inserted = m.insert(pair<const char*, int>("b", 2)).second;  // built, then dropped
    println("{} {}", inserted, m.at("b"));

    m.insert(m.begin(), {"a", 3});  // a new key goes to the end, whatever the hint
    m.insert({{"c", 4}, {"a", 5}});
    vector<pair<string, int>> more = {{"d", 6}};
    m.insert(more.begin(), more.end());
    println("{}", m);

    ordered_map<string, int> other = {{"q", 17}, {"a", 0}};
    auto moved = m.insert(other.extract("q"));  // relinked, no copy
    println("{} {} {}", moved.inserted, moved.position->first, moved.node.empty());
    auto kept = m.insert(other.extract("a"));  // the key is there: the node stays in the handle
    println("{} {} {}", kept.inserted, kept.position->second, kept.node.key());

    m.insert(m.extract("b"));  // back in, at the end of the order
    println("{}", m);
}
```

Output:

```text
b 1 true
false 1
{"b": 1, "a": 3, "c": 4, "d": 6}
true q true
false 3 a
{"a": 3, "c": 4, "d": 6, "q": 17, "b": 1}
```

## See also

- [emplace](emplace.md): constructs the element in place
- [try_emplace](try_emplace.md): builds the element only when the key is absent
- [insert_or_assign](insert_or_assign.md): inserts, or assigns to the element under the key
- [sgcl::ordered_map\<Key, T, Hash, KeyEqual\>](../ordered_map.md)
