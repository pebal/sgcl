[sgcl](../../README.md) › [core](../README.md) › [list](../list.md)

# sgcl::list\<T\>::insert

```cpp
/*(1)*/ iterator insert(const_iterator pos, const T& value)
            noexcept(std::is_nothrow_copy_constructible_v<T>);
/*(2)*/ iterator insert(const_iterator pos, T&& value)
            noexcept(std::is_nothrow_move_constructible_v<T>);
/*(3)*/ iterator insert(const_iterator pos, size_type count, const T& value)
            noexcept(std::is_nothrow_copy_constructible_v<T>);
/*(4)*/ template<std::input_iterator InputIt>
        iterator insert(const_iterator pos, InputIt first, InputIt last);
/*(5)*/ iterator insert(const_iterator pos, std::initializer_list<T> ilist);
```

Inserts elements before `pos`.

1. Inserts a copy of `value`.
2. Inserts `value`, moved.
3. Inserts `count` copies of `value`.
4. Inserts the elements of the range `[first, last)`.
5. Inserts the elements of `ilist`.

A node is made and its element constructed in one step, so no node ever holds an element that was never
constructed. Several elements (3–5) are built as a chain of nodes first and linked in at once; until then the list
is untouched, so `value` may be an element of this list, and `[first, last)` a range of it. No element of the list
is moved or copied: iterators and references to them stay valid.

## Parameters

| Parameter | Description |
|---|---|
| `pos` | the element before which the new ones go; `end()` appends |
| `value` | the value of the element to insert |
| `count` | the number of copies to insert |
| `first`, `last` | the range of elements to insert |
| `ilist` | the list of values to insert |

## Return value

- (1–2) An iterator to the inserted element.
- (3–5) An iterator to the first inserted element, or `pos` when nothing was inserted.

## Complexity

- (1–2) Constant.
- (3) Linear in `count`.
- (4) Linear in the distance between `first` and `last`.
- (5) Linear in `ilist.size()`.

## Exceptions

- (1–3) What the copy constructor (1, 3) or the move constructor (2) of `T` throws; none when it is noexcept.
- (4–5) What the constructor of `T` throws.

If an exception is thrown, the list is as it was before the call: the elements built before it are destroyed with
their chain.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    list l = {1, 4};

    auto it = l.insert(std::next(l.begin()), 2);
    println("{}, it -> {}", l, *it);

    it = l.insert(std::next(it), 2, 3);
    println("{}, it -> {}", l, *it);

    l.insert(l.begin(), {-1, 0});
    println("{}", l);

    l.insert(l.end(), l.begin(), std::next(l.begin(), 3));  // a range of l itself
    println("{}", l);
}
```

Output:

```text
[1, 2, 4], it -> 2
[1, 2, 3, 3, 4], it -> 3
[-1, 0, 1, 2, 3, 3, 4]
[-1, 0, 1, 2, 3, 3, 4, -1, 0, 1]
```

## See also

- [emplace](emplace.md): constructs an element in place
- [push_back](push_back.md), [push_front](push_front.md): add an element at an end
- [splice](splice.md): moves nodes from another list instead of copying elements
- [sgcl::list\<T\>](../list.md)
