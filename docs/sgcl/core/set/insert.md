[sgcl](../../README.md) › [core](../README.md) › [set](../set.md)

# sgcl::set\<Key, Hash, KeyEqual\>::insert

```cpp
/*(1)*/ pair<iterator, bool> insert(const value_type& value)
            noexcept(std::is_nothrow_copy_constructible_v<value_type>);
/*(2)*/ pair<iterator, bool> insert(value_type&& value)
            noexcept(std::is_nothrow_move_constructible_v<value_type>);
/*(3)*/ template<class P> requires std::is_constructible_v<value_type, P&&>
        pair<iterator, bool> insert(P&& value)
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
/*(9)*/ insert_return_type insert(node_type&& nh) noexcept;
/*(10)*/ iterator insert(const_iterator hint, node_type&& nh) noexcept;
```

Inserts elements unless their keys are there, as `std::unordered_set::insert` does.

1. Inserts a copy of `value`.
2. Inserts `value`, moved.
3. Inserts an element constructed from `value`, through [emplace](emplace.md): a `value` of another type than
   `Key` (a literal for a set of `string`) is built into an element in a new node first, and the element is
   destroyed again when its key is there.
4. As (1); the hint is ignored.
5. As (2); the hint is ignored.
6. As (3); the hint is ignored.
7. Inserts the elements of the range `[first, last)`, one after another.
8. Inserts the elements of `ilist`.
9. Links the node `nh` holds, without copying or moving the element. On success `nh` is empty afterwards; when
   the key is there, the node stays in the handle returned, and `position` is the element in the way. An empty
   handle inserts nothing.
10. As (9), the hint ignored; when the key is there, the node stays in `nh`, as it was.

- (1–2) The key is looked up in `value` itself: when it is there, nothing is built and `value` is left as it
  was.
- (1–10) The table grows before the node is linked when the size has reached `bucket_count() *
  max_load_factor()`: to a power of two at least twice the bucket count, and to 8 buckets at the least. A key
  that is there never makes the table grow.

## Parameters

| Parameter | Description |
|---|---|
| `hint` | an iterator, ignored |
| `value` | the element to insert |
| `first`, `last` | the range of the elements to insert |
| `ilist` | the list of the elements to insert |
| `nh` | a node handle from [extract](extract.md) of a set or a [multiset](../multiset.md) |

## Return value

- (1–3) The element with the key, and `true` when it was inserted. `pair` is the alias of `std::pair`
  ([aliases](../aliases.md)).
- (4–6) The element with the key.
- (7–8) None.
- (9) `position`, the element with the key; `inserted`, `true` when the node was linked; `node`, the node when
  the key was there, empty otherwise. For an empty `nh`: `end()`, `false` and an empty handle.
- (10) The element with the key, or `end()` for an empty `nh`.

## Complexity

- (1–6), (9–10) Constant on average, the walk of one bucket; a growth relinks every node, amortized constant.
- (7–8) Linear in the number of elements on average.

## Exceptions

- (1–6) What the construction of the element (the copy or the move of `Key`, its construction from `value`)
  throws; none when it is noexcept.
- (7–8) What the construction of an element from `*first` (the copy of an element of `ilist`) throws.
- (9–10) None.

If an exception is thrown, nothing is linked and the set is as it was; (7–8) keep the elements inserted before
it.

## Notes

The node-handle forms move an element between sets, or out of a [multiset](../multiset.md) and back, with no
copy of the element: [extract](extract.md) unlinks the node, `insert` links it.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    set<string> s;
    auto [it, inserted] = s.insert("a");
    println("{} {}", *it, inserted);
    println("{}", s.insert("a").second);

    s.insert(s.end(), "z");  // the hint is ignored
    s.insert({"b", "c"});
    vector<string> more = {"c", "d"};
    s.insert(more.begin(), more.end());
    println("{}", s.size());

    set<string> other = {"q", "a"};
    auto moved = s.insert(other.extract("q"));  // the node is relinked, the string not copied
    println("{} {} {}", *moved.position, moved.inserted, moved.node.empty());
    auto taken = s.insert(other.extract("a"));
    println("{} {} {}", *taken.position, taken.inserted, taken.node.value());
}
```

Output:

```text
a true
false
5
q true true
a false a
```

## See also

- [emplace](emplace.md): constructs the element in place
- [extract](extract.md): takes a node out of a set
- [merge](merge.md): relinks every node of another set
- [sgcl::set\<Key, Hash, KeyEqual\>](../set.md)
