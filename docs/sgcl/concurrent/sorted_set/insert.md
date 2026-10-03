[sgcl](../../README.md) › [concurrent](../README.md) › [sorted_set](../sorted_set.md)

# sgcl::concurrent::sorted_set\<Key, Compare\>::insert

```cpp
/*(1)*/ pair<iterator, bool> insert(const Key& key)
            noexcept(std::is_nothrow_copy_constructible_v<Key>);
/*(2)*/ pair<iterator, bool> insert(Key&& key) noexcept(std::is_nothrow_move_constructible_v<Key>);
/*(3)*/ template<std::input_iterator InputIt>
        void insert(InputIt first, InputIt last);
/*(4)*/ void insert(std::initializer_list<value_type> ilist)
            noexcept(std::is_nothrow_copy_constructible_v<Key>);
```

Inserts keys the set does not hold.

1. Inserts a copy of `key`.
2. Inserts `key`, moved.
3. Inserts the keys of the range `[first, last)`, one at a time, in their order.
4. Inserts the keys of `ilist`, one at a time.

- (1–2) One search finds the key's neighbours at every level; when the key is there, nothing is built and `key`
  is left as it was, as `std::set::insert` leaves it. When it is absent, the node is built, of a height drawn
  against the levels the search saw, and linked between the neighbours that search found: into the bottom list
  with a compare-exchange, then into its upper levels. A neighbour that changed meanwhile fails the exchange, and
  the search is done again.
- (3–4) Each key is inserted as (1–2) insert it. A key of another type than `Key` in the range is built into a
  key in a new node first, as by [emplace](emplace.md), and the node is dropped when the set holds it.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key to insert |
| `first`, `last` | the range of the keys to insert |
| `ilist` | the list of the keys to insert |

## Return value

- (1–2) A pair of an iterator and a `bool`: the inserted key and `true`, or the key already in the set and
  `false`.
- (3–4) None.

## Complexity

- (1–2) Logarithmic in the size of the set, expected, plus a search again after each compare-exchange lost to
  another thread; no allocation when the key is there.
- (3–4) The same per key.

## Exceptions

- (1–2), (4) What the copy or the move of `Key` throws; none when it is noexcept.
- (3) What the construction of `Key` from `*first` throws.

If an exception is thrown, the key that threw is not linked; with (3–4) the keys inserted before it stay.

## Notes

Lock-free, and linearizable per key at the compare-exchange that links it into the bottom list: of concurrent
insertions of one key, exactly one returns `true`. A range (3–4) is not inserted as a whole: other threads see
its keys as they are linked.

A key given by rvalue (2), or from a range of rvalues (3), is looked up first and moved only into a node of its
own. When another thread inserts
the same key between that search and the link, the search done again finds it, and the key has been moved into a
node that is dropped: the one case where a key already there is moved from. A set built from a range at once,
before any other thread sees it, is the [constructor](sorted_set.md) from the range.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::sorted_set<string> tags;
    auto [it, inserted] = tags.insert("red");
    println("{} {}", *it, inserted);
    println("{}", tags.insert("red").second);  // the key is there: nothing built

    vector<string> more = {"green", "blue", "red"};
    tags.insert(more.begin(), more.end());
    tags.insert({"cyan", "amber"});
    println("{}", tags);
}
```

Output:

```text
red true
false
{"amber", "blue", "cyan", "green", "red"}
```

## See also

- [emplace](emplace.md): constructs the key in place
- [erase](erase.md): erases a key
- [sgcl::concurrent::sorted_set\<Key, Compare\>](../sorted_set.md)
