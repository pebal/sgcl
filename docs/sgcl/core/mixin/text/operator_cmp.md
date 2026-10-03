[sgcl](../../../README.md) › [core](../../README.md) › [mixin](../README.md) › [text](README.md)

# sgcl::mixin::operator==, operator\<=\> (sgcl::mixin::text)

```cpp
friend bool operator==(const Derived& a, view_type s) noexcept;                             // (1)
template<size_t N>
friend bool operator==(const Derived& a, const CharT (&s)[N]) noexcept;                     // (2)
template<class P> requires std::same_as<P, const CharT*> || std::same_as<P, CharT*>
friend bool operator==(const Derived& a, P s) noexcept;                                     // (3)
friend std::strong_ordering operator<=>(const Derived& a, view_type s) noexcept;            // (4)
template<size_t N>
friend std::strong_ordering operator<=>(const Derived& a, const CharT (&s)[N]) noexcept;    // (5)
template<class P> requires std::same_as<P, const CharT*> || std::same_as<P, CharT*>
friend std::strong_ordering operator<=>(const Derived& a, P s) noexcept;                    // (6)
```

Compare a string or a text slice with another text by the characters, as `std::basic_string_view`'s `==` and `<=>`
do.

- (1), (4) With the view `s`: a `std::basic_string_view`, a `std::basic_string`.
- (2), (5) With the characters of an array, a literal, up to its first NUL or its end, never past it.
- (3), (6) With the characters at the pointer `s`, up to their NUL.

The operators are friends on `Derived`, found by argument-dependent lookup: an exact match on the object, so that a
literal does not also convert to `Derived` and tie. `!=`, `<`, `<=`, `>`, `>=` and the reversed forms (`"abc" ==
s`) are C++20's rewrites of these. Two strings, a string and a slice, or two slices compare by the classes' own
operators.

## Parameters

| Parameter | Description |
|---|---|
| `a` | the string or the text slice |
| `s` | the text to compare with |

## Return value

- (1–3) `true` when the two texts have the same characters, `false` otherwise.
- (4–6) The order of the two texts: by `Traits::compare` over the shorter length, then the shorter text first.

## Complexity

Linear in the length of the shorter text.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <string_view>

using namespace sgcl;

int main() {
    string s = "kot";
    const char* other = "pies";
    println("{} {} {}", s == "kot", "kot" == s, s != other);
    println("{} {} {}", s < "kotek", s > other, s >= std::string_view("kot"));

    slice<const char> tail = string("mały kot").as_slice(6);
    println("{} {}", tail == "kot", (tail <=> "kos") > 0);
}
```

Output:

```text
true true true
true false true
true true
```

## See also

- [compare](compare.md): the same comparison as an `int`, of a part of the text
- [equal_fold](equal_fold.md): the same letters in either case
- [sgcl::mixin::text\<Derived, CharT, Traits\>](README.md)
