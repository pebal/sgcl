[sgcl](../../README.md) › [core](../README.md) › [slice](../slice.md)

# sgcl::slice\<T\>::slice

```cpp
slice() noexcept;                                                                         // (1)
slice(T* first, T* last) noexcept;                                                        // (2)
slice(T* first, size_type n) noexcept;                                                    // (3)
slice(const tracked_ptr<const void>& owner, T* first, T* last) noexcept;                  // (4)
slice(const tracked_ptr<const void>& owner, T* first, size_type n) noexcept;              // (5)
slice(std::span<T> s) noexcept;                                                           // (6)
template<size_t N>
slice(T (&a)[N]) noexcept;                                                                // (7)
template<class U, size_t N>
requires std::is_convertible_v<U (*)[], T (*)[]>
slice(std::array<U, N>& a) noexcept;                                                      // (8)
template<class U, size_t N>
requires std::is_convertible_v<const U (*)[], T (*)[]>
slice(const std::array<U, N>& a) noexcept;                                                // (9)
template<class U, class A>
requires std::is_convertible_v<U (*)[], T (*)[]>
slice(std::vector<U, A>& v) noexcept;                                                     // (10)
template<class U, class A>
requires std::is_convertible_v<const U (*)[], T (*)[]>
slice(const std::vector<U, A>& v) noexcept;                                               // (11)
template<class Traits>
requires std::is_convertible_v<const std::remove_const_t<T> (*)[], T (*)[]>
slice(std::basic_string_view<std::remove_const_t<T>, Traits> s) noexcept;                 // (12)
template<class U>
requires (!std::is_same_v<U, T>) && std::is_convertible_v<U (*)[], T (*)[]>
slice(const slice<U>& o) noexcept;                                                        // (13)
template<class U>
requires std::is_same_v<T, const byte> && std::is_same_v<std::remove_const_t<U>, char>
slice(const slice<U>& text) noexcept;                                                     // (14)
template<class Traits>
requires std::is_same_v<T, const byte>
slice(const basic_string<char, Traits>& text) noexcept;                                   // (15)
template<class Traits>
requires std::is_same_v<T, const byte>
slice(std::basic_string_view<char, Traits> text) noexcept;                                // (16)
template<size_t N>
requires std::is_same_v<T, const byte>
slice(const char (&text)[N]) noexcept;                                                    // (17)
template<size_t N>
requires std::is_same_v<T, const byte>
slice(const unsigned char (&data)[N]) noexcept;                                           // (18)
template<size_t N>
requires std::is_same_v<T, const byte>
slice(const std::array<unsigned char, N>& data) noexcept;                                 // (19)
slice(const slice& o) noexcept;                                                           // (20)
slice(slice&& o) noexcept;                                                                // (21)
```

Constructs a slice.

1. An empty slice without an owner.
2. The elements `[first, last)` of unmanaged memory: no owner.
3. The elements `[first, first + n)` of unmanaged memory: no owner.
4. The elements `[first, last)` of the managed object `owner`, which the slice holds. A debug build asserts that the
   elements lie in the owner.
5. The elements `[first, first + n)` of `owner`, the same way.
6. The elements of a `std::span`: no owner.
7. The elements of an array: no owner. An array of `const` characters, a literal, is text: read up to its first NUL
   or its end, whichever comes first, so `string_slice s = "ab"` is two characters, without the terminator.
8. The elements of a `std::array`: no owner.
9. The same, of a `const std::array`, for a slice of `const` elements.
10. The elements of a `std::vector`: no owner.
11. The same, of a `const std::vector`, for a slice of `const` elements.
12. The characters of a `std::basic_string_view`, for a `slice<const CharT>`: no owner.
13. The elements of another slice whose pointer converts to `T*` (a `slice<int>` to a `slice<const int>`), the
    owner carried over.
14. The bytes of a text slice, for a `slice<const byte>`, the owner carried over.
15. The bytes of a `string`, for a `slice<const byte>`, the string's object as the owner.
16. The bytes of a `std::string_view`, for a `slice<const byte>`: no owner.
17. The bytes of a literal or an array of `char`, for a `slice<const byte>`, up to its first NUL or its end,
    whichever comes first (a literal without its terminator, a buffer filled to the brim not read past): no owner.
18. All the bytes of an array of `unsigned char` (`uint8_t`), for a `slice<const byte>`: no owner.
19. All the bytes of a `std::array<unsigned char, N>`, for a `slice<const byte>`: no owner.
20. A copy: the same elements, the same owner. A null owner is copied without the registration of the thread; a
    non-null one through `tracked_ptr`'s copy, which registers the thread's stack the first time.
21. The same as the copy: the source keeps its elements and its owner.

- (6–19) are implicit, so a function that takes a slice takes these as they are.

A [vector](../vector.md) and a [string](../string.md) of the library convert to a slice by themselves, with their
object as the owner: `f(v)`, `f(s)` for a function that takes a slice.

## Parameters

| Parameter | Description |
|---|---|
| `first`, `last` | the elements, as a range of pointers |
| `n` | the number of elements |
| `owner` | the managed object the elements lie in |
| `s`, `a`, `v` | the span, the array, the vector or the view whose elements are taken |
| `o` | the slice whose elements are taken |
| `text`, `data` | the text or the bytes whose bytes are taken |

## Complexity

Constant; (17) linear in the characters up to the first NUL.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <vector>

using namespace sgcl;

int main() {
    int local[4] = {1, 2, 3, 4};
    slice<int> on_stack(local);  // no owner
    std::vector<int> sv = {5, 6};
    slice<const int> of_std = sv;  // no owner

    vector v = {7, 8, 9};
    slice<int> of_vector = v;  // the buffer as the owner
    slice<const int> read_only = of_vector;  // the owner carried over

    string text = "key=value";
    slice<const byte> bytes = text;  // the string's object as the owner
    slice<const byte> literal = "abc";  // no terminator
    string_slice word = "abc";  // text: no terminator either

    println("{} {} {} {}", on_stack, of_std, of_vector, read_only);
    println("{} {} {}", on_stack.owned(), of_vector.owned(), read_only.owned());
    println("{} {} {} {}", bytes.size(), bytes.owned(), literal.size(), word.size());
}
```

Output:

```text
[1, 2, 3, 4] [5, 6] [7, 8, 9] [7, 8, 9]
false true true
9 true 3 3
```

## See also

- [as_slice](../vector/as_slice.md): the slice of a vector, or of a part of it
- [subslice](subslice.md): a piece of a slice, the same owner
- [sgcl::slice\<T\>](../slice.md)
