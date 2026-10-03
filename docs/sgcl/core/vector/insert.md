[sgcl](../../README.md) › [core](../README.md) › [vector](../vector.md)

# sgcl::vector\<T\>::insert

```cpp
/*(1)*/ iterator insert(const_iterator pos, const T& value)
            noexcept(std::is_nothrow_copy_constructible_v<T> &&
                     std::is_nothrow_move_constructible_v<T> &&
                     std::is_nothrow_move_assignable_v<T>);
/*(2)*/ iterator insert(const_iterator pos, T&& value)
            noexcept(std::is_nothrow_move_constructible_v<T> &&
                     std::is_nothrow_move_assignable_v<T>);
/*(3)*/ iterator insert(const_iterator pos, size_type count, const T& value);
/*(4)*/ template<std::input_iterator InputIt>
        iterator insert(const_iterator pos, InputIt first, InputIt last);
/*(5)*/ iterator insert(const_iterator pos, std::initializer_list<T> ilist);
```

Inserts elements before `pos`.

1. Inserts a copy of `value`.
2. Inserts `value`, moved.
3. Inserts `count` copies of `value`.
4. Inserts the elements of the range `[first, last)`. `first` and `last` may not be iterators into this vector.
5. Inserts the elements of `ilist`.

Within the capacity the elements from `pos` on move up to make room. Above it the new elements are constructed
in a fresh buffer first and the old ones move over after, so `value` may be an element of this vector, in every
overload. A single-pass range (4) is collected first and then inserted by moving.

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

- (1–2) Constant, plus linear in the distance between `pos` and the end.
- (3) Linear in `count`, plus linear in the distance between `pos` and the end.
- (4) Linear in the distance between `first` and `last`, plus linear in the distance between `pos` and the end.
- (5) Linear in `ilist.size()`, plus linear in the distance between `pos` and the end.

## Exceptions

- (1–2) What the copy constructor (1), the move constructor and the move assignment of `T` throw; none when they
  are noexcept.
- (3–5) `length_error` when the size would pass `max_size()`, and what the copy, the move or the assignment of
  `T` throws.

When the vector reallocates, or within the capacity when what throws is a copy, the vector is as it was before
the call. When a move or an assignment of `T` throws within the capacity, the vector stays consistent and every
element is destroyed exactly once, but the values, and after an assignment the size, have changed, as with
`std::vector`.

## Notes

A reallocation leaves the old buffer to the collector instead of freeing it: a [slice](../slice.md) taken
before the call still reads the old elements. The capacity of a growth is at least twice the old one.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <list>

using namespace sgcl;

int main() {
    vector v = {1, 4};

    auto it = v.insert(v.begin() + 1, 2);
    println("{}, it -> {}", v, *it);

    it = v.insert(v.begin() + 2, 2, 3);
    println("{}, it -> {}", v, *it);

    std::list<int> more = {5, 6};
    v.insert(v.end(), more.begin(), more.end());
    println("{}", v);

    v.insert(v.begin(), {-1, 0});
    println("{}", v);

    v.insert(v.end(), v[0]);  // an element of v itself
    println("{}", v);
}
```

Output:

```text
[1, 2, 4], it -> 2
[1, 2, 3, 3, 4], it -> 3
[1, 2, 3, 3, 4, 5, 6]
[-1, 0, 1, 2, 3, 3, 4, 5, 6]
[-1, 0, 1, 2, 3, 3, 4, 5, 6, -1]
```

## See also

- [push_back](push_back.md): appends an element
- [emplace](emplace.md), [erase](erase.md): construct an element in place, erase elements
- [sgcl::vector\<T\>](../vector.md)
