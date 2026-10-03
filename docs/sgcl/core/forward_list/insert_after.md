[sgcl](../../README.md) › [core](../README.md) › [forward_list](README.md)

# sgcl::forward_list\<T\>::insert_after

```cpp
iterator insert_after(const_iterator pos, const T& value)                     // (1)
    noexcept(std::is_nothrow_copy_constructible_v<T>);
iterator insert_after(const_iterator pos, T&& value)                          // (2)
    noexcept(std::is_nothrow_move_constructible_v<T>);
iterator insert_after(const_iterator pos, size_type count, const T& value)    // (3)
    noexcept(std::is_nothrow_copy_constructible_v<T>);
template<std::input_iterator InputIt>
iterator insert_after(const_iterator pos, InputIt first, InputIt last);       // (4)
iterator insert_after(const_iterator pos, std::initializer_list<T> ilist);    // (5)
```

Inserts elements after `pos`; [before_begin()](before_begin.md) inserts at the front.

1. Inserts a copy of `value`.
2. Inserts `value`, moved.
3. Inserts `count` copies of `value`.
4. Inserts the elements of the range `[first, last)`.
5. Inserts the elements of `ilist`.

A node is made and its element constructed in one step, so no node ever holds an element that was never
constructed. Several elements (3–5) are built as a chain of nodes first and linked in only once it is complete;
until then the list is untouched, so `value` may be an element of this list, and `[first, last)` a range of it. No
element of the list is moved or copied: iterators and references to them stay valid.

## Parameters

| Parameter | Description |
|---|---|
| `pos` | the element after which the new ones go, or `before_begin()` |
| `value` | the value of the element to insert |
| `count` | the number of copies to insert |
| `first`, `last` | the range of elements to insert |
| `ilist` | the list of values to insert |

## Return value

- (1–2) An iterator to the inserted element.
- (3–5) An iterator to the last inserted element, or `pos` when nothing was inserted.

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
    forward_list l = {1, 4};

    auto it = l.insert_after(l.begin(), 2);
    println("{}, it -> {}", l, *it);

    it = l.insert_after(it, 2, 3);  // it names the second 3
    l.insert_after(it, 9);
    println("{}", l);

    l.insert_after(l.before_begin(), {-1, 0});
    println("{}", l);
}
```

Output:

```text
[1, 2, 4], it -> 2
[1, 2, 3, 3, 9, 4]
[-1, 0, 1, 2, 3, 3, 9, 4]
```

## See also

- [emplace_after](emplace_after.md): constructs an element in place after a position
- [push_front](push_front.md): inserts an element at the beginning
- [splice_after](splice_after.md): moves nodes from another list instead of copying elements
- [sgcl::forward_list\<T\>](README.md)
