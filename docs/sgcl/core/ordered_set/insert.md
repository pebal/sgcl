[sgcl](../../README.md) › [core](../README.md) › [ordered_set](README.md)

# sgcl::ordered_set\<Key, Hash, KeyEqual\>::insert

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

Inserts elements unless they are there, as `std::unordered_set::insert` does. A new element goes to the end of
the order, the newest; an element that is there keeps its place.

1. Inserts a copy of `value`.
2. Inserts `value`, moved.
3. Inserts an element constructed from `value`: a value of another type than `Key` (a literal for a `string`).
4. As (1); `hint` is ignored.
5. As (2); `hint` is ignored.
6. As (3); `hint` is ignored.
7. Inserts the elements of the range `[first, last)`, in its order.
8. Inserts the elements of `ilist`, in its order.
9. Links the node `nh` holds, the element untouched.
10. As (9); `hint` is ignored.

- (1–2), (4–5) The element is looked up first: when it is there, nothing is built and `value` is left as it was.
- (3), (6) Go through [emplace](emplace.md): the element is built in a new node first, and destroyed again when
  it is there.
- (7–8) An element of the type `Key` is hashed and looked up where it is and copied once, into its node; a
  duplicate copies nothing. An element of another type is converted to a `Key` first, once.
- (9–10) No element is copied or moved. On success `nh` is empty after. When the element is there, the node
  stays in a handle: the `node` of the result of (9), `nh` itself for (10). An empty handle inserts nothing. The
  node goes to the end of the order, whatever its place was in the set it came from.

The table grows before a new node is linked when the size has reached `bucket_count() * max_load_factor()`; an
element that is there never grows it.

## Parameters

| Parameter | Description |
|---|---|
| `hint` | an iterator into the set, ignored |
| `value` | the element to insert |
| `first`, `last` | the range of the elements to insert |
| `ilist` | the list of the elements to insert |
| `nh` | a node handle, from [extract](extract.md) of this set or of another `ordered_set` with the same `Key` |

## Return value

- (1–3) The element and `true`, or the equal element already there and `false`. `pair` is the alias of
  `std::pair` ([aliases](../aliases.md)).
- (4–6) An iterator to the element inserted, or to the equal element already there.
- (7–8) None.
- (9) An `insert_return_type`: `position` the element inserted or the equal one already there, `inserted`
  whether the node was linked, `node` the handle when it was not. For an empty `nh`: `end()`, `false` and an
  empty handle.
- (10) An iterator to the element inserted or to the equal one already there, `end()` for an empty `nh`.

## Complexity

- (1–6), (9–10) Constant on average, linear in `size()` in the worst case; amortized over the growths of the
  table.
- (7–8) Linear in the number of elements on average.

## Exceptions

- (1–6) What the construction of the element (the copy or the move of `Key`, its construction from `value`)
  throws; none when it is noexcept.
- (7–8) What the construction of an element throws.
- (9–10) None.

If an exception is thrown, nothing is linked and the set is as it was; (7–8) keep the elements inserted before
it.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    ordered_set<int> seen;
    for (int v : {3, 1, 3, 2, 1}) {
        seen.insert(v);  // each value once, in first-seen order
    }
    auto [it, inserted] = seen.insert(1);
    println("{} {} {}", seen, *it, inserted);

    ordered_set<string> s = {"b"};
    s.insert(s.begin(), "a");  // a new element goes to the end, whatever the hint
    s.insert({"c", "a"});
    vector<string> more = {"d"};
    s.insert(more.begin(), more.end());
    println("{}", s);

    ordered_set<string> other = {"q", "a"};
    auto moved = s.insert(other.extract("q"));  // relinked, no copy
    println("{} {} {}", moved.inserted, *moved.position, moved.node.empty());
    auto kept = s.insert(other.extract("a"));  // there: the node stays in the handle
    println("{} {}", kept.inserted, kept.node.value());

    s.insert(s.extract("b"));  // back in, at the end of the order
    println("{}", s);
}
```

Output:

```text
{3, 1, 2} 1 false
{"b", "a", "c", "d"}
true q true
false a
{"a", "c", "d", "q", "b"}
```

## See also

- [emplace](emplace.md): constructs the element in place
- [extract](extract.md): takes a node out of a set
- [sgcl::ordered_set\<Key, Hash, KeyEqual\>](README.md)
