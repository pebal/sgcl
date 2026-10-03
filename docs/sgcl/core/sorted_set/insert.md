[sgcl](../../README.md) › [core](../README.md) › [sorted_set](../sorted_set.md)

# sgcl::sorted_set\<Key, Compare\>::insert

```cpp
pair<iterator, bool> insert(const value_type& value)               // (1)
    noexcept(std::is_nothrow_copy_constructible_v<value_type>);
pair<iterator, bool> insert(value_type&& value)                    // (2)
    noexcept(std::is_nothrow_move_constructible_v<value_type>);
iterator insert(const_iterator hint, const value_type& value)      // (3)
    noexcept(std::is_nothrow_copy_constructible_v<value_type>);
iterator insert(const_iterator hint, value_type&& value)           // (4)
    noexcept(std::is_nothrow_move_constructible_v<value_type>);
template<std::input_iterator InputIt>
void insert(InputIt first, InputIt last);                          // (5)
void insert(std::initializer_list<value_type> ilist);              // (6)
insert_return_type insert(node_type&& nh) noexcept;                // (7)
iterator insert(const_iterator hint, node_type&& nh) noexcept;     // (8)
```

Inserts elements whose keys are not in the set yet, as `std::set::insert` does.

1. Inserts a copy of `value`.
2. Inserts `value`, moved.
3. Inserts a copy of `value`, with `hint` as the place to try first.
4. Inserts `value`, moved, with `hint` as the place to try first.
5. Inserts the elements of the range `[first, last)`, one by one, with `end()` as the hint.
6. Inserts the elements of `ilist`, as (5).
7. Links the node `nh` owns into the tree: the element is neither copied nor moved.
8. The same, with `hint` as the place to try first.

- (1–4) The place of the key is found first, and the node is made only when the key is absent: for a key already
  there nothing is built, and `value` is left as it was.
- (3–4), (8) The hint is used when the key belongs right before `hint`, or right after the largest element when
  `hint` is `end()`: an append in sorted order at `end()` costs one comparison. Elsewhere the key is searched from
  the root.
- (5–6) An element of the type `Key` is compared where it is and copied into its node once its place is known. An
  element of another type (a `string_view` for `string` keys) is converted once, into a new node, and the node's
  key compared, as [emplace_hint](emplace_hint.md) does: the source need not be comparable with the keys, and no
  temporary key is built per comparison. A node whose key is taken has its element destroyed again.
- (7–8) An empty `nh` inserts nothing. When the key is taken, the node stays with the handle: in the `node` of the
  result for (7), in `nh` for (8).

## Parameters

| Parameter | Description |
|---|---|
| `value` | the element to insert |
| `hint` | an iterator to the element before which the new one is expected, or `end()` |
| `first`, `last` | the range of the elements to insert |
| `ilist` | the list of the elements to insert |
| `nh` | a node handle, from [extract](extract.md) of a sorted_set or a sorted_multiset of the same `Key`, whatever its `Compare` |

## Return value

- (1–2) The element with the key and `true`, or the element already there and `false`. `pair` is the
  alias of `std::pair` ([aliases](../aliases.md)).
- (3–4) An iterator to the element with the key, inserted or already there.
- (5–6) None.
- (7) An `insert_return_type`: `{position, true, empty}` when the node was linked, `{the element in the way,
  false, the node}` when the key was taken, `{end(), false, empty}` for an empty `nh`.
- (8) An iterator to the element with the key, inserted or already there; `end()` for an empty `nh`.

## Complexity

- (1–2), (7) Logarithmic in the size of the set.
- (3–4), (8) Amortized constant when the element goes right before `hint`, logarithmic otherwise.
- (5–6) *N* log(*size* + *N*) for *N* elements; linear in *N* when they come sorted.

## Exceptions

- (1–4) What the copy or the move of `Key` throws; none when it is noexcept.
- (5–6) What the construction of an element from `*first` (from an element of `ilist`) throws.
- (7–8) None.

If an exception is thrown, nothing is inserted by that element: (1–4) leave the set as it was, (5–6) keep the
elements inserted before it.

## Notes

An insertion stores the tracked pointers of the links it changes and pays the write barrier on each, the
rebalancing included; the search before it reads raw pointers only. No iterator is invalidated.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    sorted_set<string> names;

    auto [it, inserted] = names.insert("ann");
    println("{} {}", *it, inserted);
    println("{}", names.insert("ann").second);

    names.insert(names.end(), "zoe");  // an append: one comparison
    names.insert({"bob", "cid"});
    println("{}", names);

    sorted_set<string> other = {"dan"};
    auto result = names.insert(other.extract("dan"));  // relinked, no string copied
    println("{} {} {}", *result.position, result.inserted, result.node.empty());
    println("{} {}", names, other.empty());
}
```

Output:

```text
ann true
false
{"ann", "bob", "cid", "zoe"}
dan true true
{"ann", "bob", "cid", "dan", "zoe"} true
```

## See also

- [emplace](emplace.md): constructs the element in place
- [extract](extract.md): takes a node out of a set
- [merge](merge.md): relinks the nodes of another set
- [sgcl::sorted_set\<Key, Compare\>](../sorted_set.md)
