[sgcl](../../README.md) › [concurrent](../README.md) › [set](../set.md)

# sgcl::concurrent::set\<Key, Hash, KeyEqual\>::insert

```cpp
/*(1)*/ pair<iterator, bool> insert(const Key& key)
            noexcept(std::is_nothrow_copy_constructible_v<Key>);
/*(2)*/ pair<iterator, bool> insert(Key&& key) noexcept(std::is_nothrow_move_constructible_v<Key>);
/*(3)*/ template<std::input_iterator InputIt>
        void insert(InputIt first, InputIt last);
/*(4)*/ void insert(std::initializer_list<value_type> ilist)
            noexcept(std::is_nothrow_copy_constructible_v<Key>);
```

Inserts keys unless they are there, as `std::unordered_set::insert` does.

1. Inserts a copy of `key`.
2. Inserts `key`, moved.
3. Inserts the keys of the range `[first, last)`, one after another.
4. Inserts the keys of `ilist`.

- (1–2) Search once: when the key is there, nothing is built and `key` is left as it was, as
  `std::unordered_set::insert` leaves it. When it is absent, the node is built and linked where that search found
  its place, with a compare-exchange.
- (3–4) Each key is inserted as (1–2) insert it. A key of another type than `Key` in the range is built into a
  key in a new node first, as by [emplace](emplace.md), and the node is dropped when the set holds it.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key to insert |
| `first`, `last` | the range of the keys to insert |
| `ilist` | the list of the keys to insert |

## Return value

- (1–2) The element and `true`, or the element already equal to the key and `false`. `pair` is the alias of
  `std::pair` ([aliases](../../core/aliases.md)).
- (3–4) None.

## Complexity

- (1–2) Constant on average: one search of the key's bucket and one compare-exchange; amortized over the doublings
  of the array.
- (3–4) Linear in the number of keys, a search each.

## Exceptions

- (1–2), (4) What the copy or the move of `Key` throws; none when it is noexcept.
- (3) What the construction of `Key` from `*first` throws.

If an exception is thrown, nothing is linked and the set is as it was; (3–4) keep the keys inserted before it.

## Notes

Lock-free, and linearizable at the compare-exchange that links the node: of several threads inserting one key,
exactly one gets `true`, and the others the element it inserted. A key given by rvalue (2), or from a range of
rvalues (3), is looked up first and moved only into a node of its own. When another thread inserts the same key
between the search and the link, the compare-exchange fails, the search runs again and finds that key: the key
has been moved into a node that is dropped, the one case where the standard's promise of no move for a key
already there does not hold. The constructors from a range build the whole list at once, faster than an
insertion per key into an empty set ([(constructor)](set.md)).

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::set<string> tags;

    auto [it, inserted] = tags.insert("red");
    println("{} {}", *it, inserted);

    string again = "red";
    auto [kept, fresh] = tags.insert(std::move(again));
    println("{} {} '{}'", *kept, fresh, again);  // a key there: again not moved from

    tags.insert({"green", "blue"});
    vector<string> more = {"blue", "white"};
    tags.insert(more.begin(), more.end());
    println("{}", tags.size());
}
```

Output:

```text
red true
red false 'red'
4
```

## See also

- [emplace](emplace.md): constructs the key in place
- [erase](erase.md): erases a key
- [sgcl::concurrent::set\<Key, Hash, KeyEqual\>](../set.md)
