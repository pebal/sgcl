[sgcl](../../README.md) › [core](../README.md) › [string](../string.md)

# sgcl::string::string

```cpp
constexpr basic_string() noexcept = default;                                           // (1)
constexpr basic_string(std::nullptr_t) = delete;                                       // (2)
template<size_t N> basic_string(const CharT (&s)[N]);                                  // (3)
template<class P> requires std::same_as<P, const CharT*> || std::same_as<P, CharT*>
basic_string(P s);                                                                     // (4)
basic_string(const CharT* s, size_type n);                                             // (5)
basic_string(view_type s);                                                             // (6)
template<class V>
requires std::is_convertible_v<const V&, view_type>
      && (!std::is_convertible_v<const V&, const CharT*>)
      && (!std::is_same_v<std::remove_cvref_t<V>, basic_string>)
explicit basic_string(const V& v);                                                     // (7)
basic_string(size_type n, CharT c);                                                    // (8)
template<std::input_iterator It> basic_string(It first, It last);                      // (9)
template<std::forward_iterator It> basic_string(It first, It last);                    // (10)
basic_string(std::initializer_list<CharT> il);                                         // (11)
explicit basic_string(const slice_type& v);                                            // (12)
template<class B>
requires (sizeof(CharT) == 1) && std::is_convertible_v<const B&, slice<const byte>>
      && (!std::is_convertible_v<const B&, view_type>)
      && (!std::is_convertible_v<const B&, const CharT*>)
      && (!std::is_same_v<std::remove_cvref_t<B>, basic_string>)
      && (!std::is_same_v<std::remove_cvref_t<B>, slice_type>)
explicit basic_string(const B& bytes);                                                 // (13)
basic_string(const basic_string&) noexcept = default;                                  // (14)
basic_string(basic_string&&) noexcept = default;                                       // (15)
```

Constructs a string from what a `std::string` is made of. The characters are copied once into the string's object,
made at its size on the managed heap; a string of no characters, from whichever source, is the empty string, which is
null and allocates nothing.

1. The empty string.
2. Deleted: `string(nullptr)` does not compile, nor does `string s = 0`, which a `std::string` before C++23 takes and
   reads through.
3. The characters of the array `s` (a literal) up to its first NUL or its end, whichever comes first: an array filled
   to the brim has no NUL and is not read past its end. The better match for an array than (4), so an array never
   decays into a pointer read by `strlen`.
4. The characters `s` points at, up to its NUL. Takes part only for `CharT*` and `const CharT*`: no other pointer
   converts (`const unsigned char*`, `const int*`).
5. The `n` characters at `s`, NULs included.
6. The characters of the view `s`.
7. The characters of anything a `view_type` is made of, as `std::string` takes it: a `std::basic_string`, a type of
   the program's that converts to a view. Explicit, as in `std`. Takes part only for what does not convert to a
   pointer and is not a string; a slice of the string's characters takes (12), the better match.
8. `n` copies of the character `c`.
9. The characters of the range `[first, last)` of a single-pass iterator: gathered first, as their number is not
   known, then copied once into the string's object.
10. The characters of the range `[first, last)` of a forward iterator: counted first, and written in place into an
    object of that size.
11. The characters of `il`.
12. The characters of the slice `v`. When the slice is the whole of a string, the result is that string's object
    again, with no copy; a part of a string, or a slice of anything else (a vector's characters), is copied into a
    new string. Explicit: a string from a slice may be a copy.
13. The bytes of `bytes`, anything that converts to `slice<const byte>` and not to a view or a pointer (a slice of
    bytes, a `vector<byte>`, an `array<byte, N>`, a `std::vector<std::byte>`), taken for the characters as they
    are: what a decryption, a decoding or a file read gave back, when it is text. Nothing checks that they are
    UTF-8; `txt::decode` with the strict policy does. Takes part only for a string of one-byte characters
    (`string`, `u8string`), and explicit, since bytes are not always text: `string text(opened)`.
14. A copy of the word: the same object as the string copied.
15. A copy of the word as well, as the move of a `tracked_ptr` is: the moved-from string keeps its value.

## Parameters

| Parameter | Description |
|---|---|
| `s` | the characters: an array, a pointer to a NUL-terminated text, a pointer to `n` characters, a view |
| `n` | (5) the number of characters at `s`; (8) the number of copies of `c` |
| `v` | (7) a value a view is made of; (12) a slice of characters |
| `c` | the character repeated |
| `first`, `last` | the range of the characters |
| `il` | the list of the characters |
| `bytes` | the bytes taken for the characters |

## Complexity

- (1), (14–15) Constant.
- (3–11), (13) Linear in the number of characters: one allocation, none for the empty string.
- (12) Constant when the slice is the whole of a string; linear in its size otherwise.

## Exceptions

- (1), (14–15) None.
- (3–13) `length_error` when the characters are more than [max_size()](max_size.md), before the string's object is
  made.
- (9–10) What the iterators throw.

## Notes

A string is made once, at its size. A text that comes in pieces is gathered and made a string once:
[concat](concat.md) of a few known pieces, [join](join.md) of a range of them, each one allocation, rather than a
string grown by `+`.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <list>
#include <string>

using namespace sgcl;

int main() {
    string empty;
    string greeting = "hello, world";
    char brim[3] = {'a', 'b', 'c'};  // no NUL: read to the end of the array
    string letters = brim;
    string hello("hello, world", 5);
    string stars(3, '*');
    std::list<char> chars = {'x', 'y', 'z'};
    string from_list(chars.begin(), chars.end());
    string listed = {'o', 'k'};
    std::string built = "std";
    string from_std(built);
    println("{} {} {} {} {} {} {} {}", empty.size(), greeting, letters, hello, stars, from_list,
            listed, from_std);

    string_slice whole = greeting;
    string again(whole);
    string world(greeting.as_slice(7));
    println("{} {} {}", again.object() == greeting.object(), world,
            world.object() == greeting.object());

    vector<byte> payload = {byte('h'), byte('i')};
    string text(payload);
    string copy = greeting;
    string moved = std::move(copy);
    println("{} {} {} {}", text, copy.object() == greeting.object(), moved, copy);
}
```

Output:

```text
0 hello, world abc hello *** xyz ok std
true world false
hi true hello, world hello, world
```

## See also

- [operator=](operator_assign.md): assigns another string, or a new one made of characters
- [as_slice, operator slice_type](as_slice.md): the characters as a slice that holds the object
- [concat](concat.md), [join](join.md): one string of pieces, made once
- [to_string](../to_string.md): a number as a string
- [sgcl::string](../string.md)
