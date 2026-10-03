[sgcl](../../../README.md) › [core](../../README.md) › [mixin](../README.md) › [text](README.md)

# sgcl::mixin::text\<Derived, CharT, Traits\>::compare

```cpp
int compare(view_type s) const noexcept;                                                     // (1)
int compare(size_type pos, size_type n, view_type s) const;                                  // (2)
int compare(size_type pos, size_type n, view_type s, size_type pos2, size_type n2) const;    // (3)
template<size_t N> int compare(size_type pos, size_type n, const CharT (&s)[N]) const;       // (4)
template<size_t N>
int compare(size_type pos, size_type n, const CharT (&s)[N], size_type pos2,                 // (5)
            size_type n2) const;
template<size_t N> int compare(const CharT (&s)[N]) const noexcept;                          // (6)
template<class P> requires std::same_as<P, const CharT*> || std::same_as<P, CharT*>
int compare(P s) const noexcept;                                                             // (7)
```

Compares the text, or its part `[pos, pos + n)`, with another text, as `std::basic_string_view::compare` does: by
`Traits::compare` over the shorter length, then the shorter text first. A part reaches at most to the end of its
text.

1. The text with the view `s`: a string, a text slice, a `std::basic_string_view` and a `std::basic_string` convert
   to it.
2. The part `[pos, pos + n)` with `s`.
3. The part `[pos, pos + n)` with the part `[pos2, pos2 + n2)` of `s`.
4. As (2), with `s` an array of characters, a literal.
5. As (3), with `s` an array of characters.
6. The text with an array of characters.
7. The text with the characters at the pointer `s`, up to their NUL.

- (4–6) An array is read up to its first NUL or its end, never past it.

## Parameters

| Parameter | Description |
|---|---|
| `s` | the text to compare with |
| `pos`, `n` | the position and the length of the part of this text; `npos` for `n` is to the end |
| `pos2`, `n2` | the position and the length of the part of `s` |

## Return value

A negative value when the text, or its part, comes before `s` (or its part), zero when they are equal, a positive
value when it comes after.

## Complexity

Linear in the length of the shorter of the two texts compared.

## Exceptions

- (1), (6–7) None.
- (2), (4) `out_of_range` when `pos > size()`.
- (3), (5) `out_of_range` when `pos > size()` or `pos2` is past the end of `s`.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string s = "the quick brown fox";
    println("{} {}", s.compare("the quick brown fox"), s.compare("the slow") < 0);
    println("{} {}", s.compare(4, 5, "quick"), s.compare(4, 5, "a quick one", 2, 5));

    const char* fox = "fox";
    println("{} {}", s.as_slice(16).compare(fox), s.compare(s.as_slice(4)) > 0);

    try {
        s.compare(20, 1, "x");
    } catch (const out_of_range&) {
        println("out of range");
    }
}
```

Output:

```text
0 true
0 0
0 true
out of range
```

## See also

- [operator==, operator\<=\>](operator_cmp.md): the comparisons with a view, a literal or a pointer as operators
- [equal_fold](equal_fold.md): the same letters in either case
- [sgcl::mixin::text\<Derived, CharT, Traits\>](README.md)
