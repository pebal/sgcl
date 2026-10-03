[sgcl](../../README.md) › [core](../README.md) › [deque](../deque.md)

# sgcl::deque\<T\>::insert

```cpp
iterator insert(const_iterator pos, const T& value)                      // (1)
    noexcept(std::is_nothrow_constructible_v<T, const T&> &&
             std::is_nothrow_move_constructible_v<T> &&
             std::is_nothrow_move_assignable_v<T>);
iterator insert(const_iterator pos, T&& value)                           // (2)
    noexcept(std::is_nothrow_constructible_v<T, T&&> &&
             std::is_nothrow_move_constructible_v<T> &&
             std::is_nothrow_move_assignable_v<T>);
iterator insert(const_iterator pos, size_type count, const T& value);    // (3)
template<std::input_iterator InputIt>
iterator insert(const_iterator pos, InputIt first, InputIt last);        // (4)
iterator insert(const_iterator pos, std::initializer_list<T> ilist);     // (5)
```

Inserts elements before `pos`.

1. Inserts a copy of `value`.
2. Inserts `value`, moved.
3. Inserts `count` copies of `value`.
4. Inserts the elements of the range `[first, last)`. `first` and `last` may not be iterators into this deque.
5. Inserts the elements of `ilist`.

An insertion at either end is a push there. In the middle the shorter side of the deque shifts by the count: the
new elements are pushed at the nearer end and moved into place; a range of forward-only iterators (4) pushed at
the front goes in as it comes, and the new elements are then reversed among themselves, so the elements already
there stay where they are. The value is built before anything moves, so
`value` (1–3) may be an element of this deque. A single-pass range (4) is collected first and then inserted by
moving.

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

- (1–2) Constant at either end; otherwise linear in the distance between `pos` and the nearer end.
- (3) Linear in `count`, plus linear in the distance between `pos` and the nearer end.
- (4) Linear in the distance between `first` and `last`, plus linear in the distance between `pos` and the nearer
  end.
- (5) Linear in `ilist.size()`, plus linear in the distance between `pos` and the nearer end.

## Exceptions

- (1–2) What the copy constructor (1), the move constructor and the move assignment of `T` throw; none when they
  are noexcept.
- (3–5) `length_error` when the size would pass `max_size()`, before anything is inserted, and what the copy, the
  move or the assignment of `T` throws.

When what throws is the construction of an inserted element, the elements pushed before it are popped again and
the deque is as it was before the call; a failure while a single-pass range (4) is collected leaves it untouched.
When a move or an assignment of `T` throws while the elements are moved into place, the deque stays consistent
and every element is destroyed exactly once, but the values, and the size, may have changed.

## Notes

An insertion at either end keeps the references to the other elements valid and invalidates the iterators, as
`std::deque`'s does, a range of forward-only iterators included; an insertion in the middle invalidates both
([Iterator invalidation](../deque.md#iterator-invalidation)).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <forward_list>
#include <list>

using namespace sgcl;

int main() {
    deque d = {1, 4};

    auto it = d.insert(d.begin() + 1, 2);
    println("{}, it -> {}", d, *it);

    it = d.insert(d.begin() + 2, 2, 3);
    println("{}, it -> {}", d, *it);

    std::list<int> more = {5, 6};
    d.insert(d.end(), more.begin(), more.end());
    println("{}", d);

    std::forward_list<int> front = {-3, -2};
    int& first = d[0];
    d.insert(d.begin(), front.begin(), front.end());  // forward only, at the front
    println("{} {}", d, first);
    d.erase(d.begin(), d.begin() + 2);

    d.insert(d.begin(), {-1, 0});
    println("{}", d);

    d.insert(d.end(), d[0]);  // an element of d itself
    println("{}", d);
}
```

Output:

```text
[1, 2, 4], it -> 2
[1, 2, 3, 3, 4], it -> 3
[1, 2, 3, 3, 4, 5, 6]
[-3, -2, 1, 2, 3, 3, 4, 5, 6] 1
[-1, 0, 1, 2, 3, 3, 4, 5, 6]
[-1, 0, 1, 2, 3, 3, 4, 5, 6, -1]
```

## See also

- [emplace](emplace.md): constructs an element in place
- [push_back](push_back.md), [push_front](push_front.md): insert an element at either end
- [erase](erase.md): erases elements
- [sgcl::deque\<T\>](../deque.md)
