[sgcl](../../README.md) › [concurrent](../README.md) › [sorted_map](../sorted_map.md)

# sgcl::concurrent::sorted_map\<Key, T, Compare\>::insert

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

Inserts elements whose keys the map does not hold.

1. Inserts a copy of `value`.
2. Inserts `value`, moved.
3. Inserts an element constructed from `value`: a pair of other types, `pair<int, const char*>` for a map of
   `int` to `string`.
4. Inserts the elements of the range `[first, last)`, one at a time, in their order.
5. Inserts the elements of `ilist`, one at a time.

- (1–2) One search finds the key's neighbours at every level; when the key is there, nothing is built and `value`
  is left as it was, as `std::map::insert` leaves it. When the key is absent, the node is built, of a height
  drawn against the levels the search saw, and linked between the neighbours that search found: into the bottom
  list with a compare-exchange, then into its upper levels. A neighbour that changed meanwhile fails the
  exchange, and the search is done again.
- (3) The same for a pair whose first is of the type `Key`. A pair with a key of another type
  (`pair<const char*, int>` for a map of `string`) is built into an element in a new node first, as by
  [emplace](emplace.md), and the node is dropped when the key is taken.
- (4–5) Each element is inserted as (1–3) insert it.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the element to insert |
| `first`, `last` | the range of the elements to insert |
| `ilist` | the list of the elements to insert |

## Return value

- (1–3) A pair of an iterator and a `bool`: the inserted element and `true`, or the element already under the key
  and `false`.
- (4–5) None.

## Complexity

- (1–3) Logarithmic in the size of the map, expected, plus a search again after each compare-exchange lost to
  another thread; no allocation when the key is there, except for (3) with a key of another type.
- (4–5) The same per element.

## Exceptions

- (1–3), (5) What the construction of `value_type` (its copy, its move, its construction from `value`)
  throws; none when it is noexcept.
- (4) What the construction of `value_type` from `*first` throws.

If an exception is thrown, the element that threw is not linked; with (4–5) the elements inserted before it
stay.

## Notes

Lock-free, and linearizable per element at the compare-exchange that links it into the bottom list: of
concurrent insertions of one key, exactly one returns `true`. A range (4–5) is not inserted as a whole: other
threads see its elements as they are linked, and their own insertions land between them.

A `value` given by rvalue (2), or (3) with a first of the type `Key`, is looked up first and moved only into a
node of its own. When another thread inserts the same key between that search and the link, the search done
again finds it, and `value` has been moved into a node that is dropped: the one case where an element whose key
is taken is moved from. A map built from a
range at once, before any other thread sees it, is the [constructor](sorted_map.md) from the range: a store per
link and no search.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::sorted_map<int, string> names;
    auto [it, inserted] = names.insert({2, "Grace"});
    println("{} {}", it->second, inserted);

    pair<int, string> linus(2, "Linus");
    auto again = names.insert(std::move(linus));  // the key is taken: linus is left as it was
    println("{} {} {}", again.first->second, again.second, linus.second);

    names.insert(pair<int, const char*>(1, "Ada"));
    names.insert({{3, "Edsger"}, {4, "Barbara"}});
    println("{}", names);
}
```

Output:

```text
Grace true
Grace false Linus
{1: "Ada", 2: "Grace", 3: "Edsger", 4: "Barbara"}
```

## See also

- [emplace](emplace.md): constructs the element in place
- [try_emplace](try_emplace.md): builds the element only when the key is absent
- [erase](erase.md): erases an element
- [sgcl::concurrent::sorted_map\<Key, T, Compare\>](../sorted_map.md)
