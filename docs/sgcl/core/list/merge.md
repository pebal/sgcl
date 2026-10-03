[sgcl](../../README.md) › [core](../README.md) › [list](../list.md)

# sgcl::list\<T\>::merge

```cpp
/*(1)*/ void merge(list& other) noexcept(/* see below */);
/*(2)*/ void merge(list&& other) noexcept(/* see below */);
/*(3)*/ template<class Compare>
        void merge(list& other, Compare comp)
            noexcept(std::is_nothrow_invocable_v<Compare&, T&, T&>);
/*(4)*/ template<class Compare>
        void merge(list&& other, Compare comp) noexcept(/* see below */);
```

Merges the sorted list `other` into this sorted list, which stays sorted; `other` is empty after.

- (1–2) The elements are ordered by `<`.
- (3–4) The elements are ordered by `comp`, which returns `true` when its first argument goes before the second.

The merge is stable: of equal elements, those of this list come first, and each list's keep their order. No element
is copied or moved: the nodes of `other` are relinked, a run of them that goes before the same element of this list
in one relink, and iterators and references to them stay valid, naming elements of this list now. Merging a list
with itself does nothing.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the sorted list to merge in |
| `comp` | the order of the elements, `bool comp(const T&, const T&)` |

## Return value

None.

## Complexity

Linear in `size() + other.size()`: one comparison per step along either list.

## Exceptions

- (1–2) What `<` of the elements throws; none when it is noexcept, as for `int`.
- (3) What `comp` throws; none when its call is noexcept.
- (4) What `comp` or its copy throws; none when both are noexcept.

The sizes follow every relink, so a throwing comparison leaves two valid lists: the nodes merged before it are in
this list, the rest in `other`.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    list a = {1, 3, 5};
    list b = {2, 4, 6};
    int& four = *std::next(b.begin());
    a.merge(b);
    println("{} {}", a, b);

    four = 40;  // the node moved to a, the reference still names it
    println("{}", a);

    list x = {9, 5, 1};
    list y = {8, 2};
    x.merge(y, std::greater<>());  // both lists in descending order
    println("{}", x);
}
```

Output:

```text
[1, 2, 3, 4, 5, 6] []
[1, 2, 3, 40, 5, 6]
[9, 8, 5, 2, 1]
```

## See also

- [sort](sort.md): sorts the list, by the same order
- [splice](splice.md): moves nodes without comparing them
- [sgcl::list\<T\>](../list.md)
