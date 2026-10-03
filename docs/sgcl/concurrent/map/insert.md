[sgcl](../../README.md) › [concurrent](../README.md) › [map](README.md)

# sgcl::concurrent::map\<Key, T, Hash, KeyEqual\>::insert

```cpp
pair<iterator, bool> insert(const value_type& value)                   // (1)
    noexcept(std::is_nothrow_copy_constructible_v<value_type>);
pair<iterator, bool> insert(value_type&& value)                        // (2)
    noexcept(std::is_nothrow_move_constructible_v<value_type>);
template<class P> requires std::is_constructible_v<value_type, P&&>
pair<iterator, bool> insert(P&& value)                                 // (3)
    noexcept(std::is_nothrow_constructible_v<value_type, P&&>);
template<std::input_iterator InputIt>
void insert(InputIt first, InputIt last);                              // (4)
void insert(std::initializer_list<value_type> ilist)                   // (5)
    noexcept(std::is_nothrow_copy_constructible_v<value_type>);
```

Inserts elements unless their keys are taken, as `std::unordered_map::insert` does.

1. Inserts a copy of `value`.
2. Inserts `value`, moved.
3. Inserts an element constructed from `value`: a `pair` of other types than `value_type`'s.
4. Inserts the elements of the range `[first, last)`, one after another.
5. Inserts the elements of `ilist`.

- (1–2) One search of the key's bucket; when the key is there, nothing is built and `value` is left as it was, as
  `std::unordered_map::insert` leaves it. When the key is absent, the node is built and linked where that search
  found its place, at its split key, with a compare-exchange; a neighbour that changed meanwhile fails the
  exchange, and the search is done again.
- (3) The same for a pair whose first is of the type `Key` (`pair<string, int>` for a map of `string` to `int`). A
  pair with a key of another type (`pair<const char*, int>`) is built into an element in a new node first, as by
  [emplace](emplace.md), and the node is dropped when the key is taken.
- (4–5) Each element is inserted as (1–3) insert it.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the element to insert |
| `first`, `last` | the range of the elements to insert |
| `ilist` | the list of the elements to insert |

## Return value

- (1–3) The element and `true`, or the element already under the key and `false`. `pair` is the alias of
  `std::pair` ([aliases](../../core/aliases.md)).
- (4–5) None.

## Complexity

- (1–3) Constant on average: the search of the key's bucket and one compare-exchange; amortized over the
  doublings of the array. No allocation when the key is there, except for (3) with a key of another type.
- (4–5) Linear in the number of elements, a search each.

## Exceptions

- (1–3), (5) What the construction of the element (the copy or the move of `value_type`, its construction
  from `value`) throws; none when it is noexcept.
- (4) What the construction of the element from `*first` throws.

If an exception is thrown, nothing is linked and the map is as it was; (4–5) keep the elements inserted before
it.

## Notes

Lock-free, and linearizable at the compare-exchange that links the node: of several threads inserting one key,
exactly one gets `true`, and the others the element it inserted. A `value` given by rvalue (2), or (3) with a
first of the type `Key`, is looked up first and moved only into a node of its own. When another thread inserts
the same key between the search and the link, the compare-exchange fails, the search runs again and finds that
key: `value` has been moved into a node that is dropped, the one case where an element whose key is taken is
moved from. The constructors from a range build the whole list at once, faster than an insertion per element
into an empty map ([(constructor)](map.md)).

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::map<string, int> ports;

    auto [it, inserted] = ports.insert({"http", 80});
    println("{} {} {}", it->first, it->second, inserted);

    pair<string, int> alternate("http", 8080);
    auto [kept, again] = ports.insert(std::move(alternate));  // the key is taken
    println("{} {} {}", kept->second, again, alternate.first);

    ports.insert({{"https", 443}, {"ssh", 22}});
    vector<pair<string, int>> more = {{"smtp", 25}, {"ssh", 2222}};
    ports.insert(more.begin(), more.end());
    println("{} {}", ports.size(), ports.value_or("ssh", 0));
}
```

Output:

```text
http 80 true
80 false http
4 22
```

## See also

- [emplace](emplace.md): constructs the element in place
- [try_emplace](try_emplace.md): builds the element only when the key is absent
- [sgcl::concurrent::map\<Key, T, Hash, KeyEqual\>](README.md)
