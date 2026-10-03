[sgcl](../../README.md) › [core](../README.md) › [string](README.md)

# sgcl::operator+ (sgcl::string)

```cpp
namespace sgcl {
    template<class CharT, class Traits>
    basic_string<CharT, Traits> operator+(const basic_string<CharT, Traits>& a,                              // (1)
                                          const basic_string<CharT, Traits>& b);
    template<class CharT, class Traits>
    basic_string<CharT, Traits> operator+(const basic_string<CharT, Traits>& a,                              // (2)
                                          std::type_identity_t<std::basic_string_view<CharT, Traits>> b);
    template<class CharT, class Traits>
    basic_string<CharT, Traits> operator+(std::type_identity_t<std::basic_string_view<CharT, Traits>> a,     // (3)
                                          const basic_string<CharT, Traits>& b);
    template<class CharT, class Traits, size_t N>
    basic_string<CharT, Traits> operator+(const basic_string<CharT, Traits>& a,                              // (4)
                                          const CharT (&b)[N]);
    template<class CharT, class Traits, class P>
    requires std::same_as<P, const CharT*> || std::same_as<P, CharT*>
    basic_string<CharT, Traits> operator+(const basic_string<CharT, Traits>& a, P b);                        // (5)
    template<class CharT, class Traits, size_t N>
    basic_string<CharT, Traits> operator+(const CharT (&a)[N],                                               // (6)
                                          const basic_string<CharT, Traits>& b);
    template<class CharT, class Traits, class P>
    requires std::same_as<P, const CharT*> || std::same_as<P, CharT*>
    basic_string<CharT, Traits> operator+(P a, const basic_string<CharT, Traits>& b);                        // (7)
    template<class CharT, class Traits>
    basic_string<CharT, Traits> operator+(const basic_string<CharT, Traits>& a, CharT b);                    // (8)
    template<class CharT, class Traits>
    basic_string<CharT, Traits> operator+(CharT a, const basic_string<CharT, Traits>& b);                    // (9)
}
```

Returns a new string of the characters of `a` followed by those of `b`, built once: the two lengths are summed, and
both texts are written into one object of that size. The result is a new object even when one side is empty; only
an empty result is the empty string, with no object.

1. Two strings.
2. A string and a view: a `std::basic_string_view`, or anything that converts to one, such as a `std::basic_string`
   or a text slice.
3. A view and a string.
4. A string and an array (a literal), read up to its first NUL or its end, whichever comes first.
5. A string and a pointer, `CharT*` or `const CharT*`, read up to its NUL.
6. An array (a literal) and a string.
7. A pointer and a string.
8. A string and a character.
9. A character and a string.

- (8–9) The character is a `CharT`: a `char32_t` or an `int` (`'ż'` is an `int`) does not compile.

## Parameters

| Parameter | Description |
|---|---|
| `a` | the text that comes first |
| `b` | the text that comes second |

## Return value

The new string.

## Complexity

Linear in the sum of the lengths: one allocation, none when both are empty.

## Exceptions

`length_error` when the sum of the lengths is above [max_size()](max_size.md), before anything is allocated.

## Notes

A string is never grown by `a + b + c`, or by `+` in a loop: each `+` is a string of its own, made and dropped, and
its characters are copied again by the next one, so a loop of `+` costs the square of the length. A few known pieces
are one string by [concat](concat.md), a range of them by [join](join.md): their lengths summed first, and each
character written once.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <string>

using namespace sgcl;

int main() {
    string name = "alice";
    std::string domain = "example.com";
    string greeting = "hello, " + name;
    string exclaimed = greeting + '!';
    string address = name + domain;
    println("{} | {} | {}", greeting, exclaimed, address);

    string email = string::concat(name, '@', domain);  // three pieces: one string, not two
    println("{} {}", email, (name + "").object() == name.object());
}
```

Output:

```text
hello, alice | hello, alice! | aliceexample.com
alice@example.com false
```

## See also

- [concat](concat.md): one string of a few pieces in order, made once
- [join](join.md): one string of the parts with a separator between each two
- [sgcl::string](README.md)
