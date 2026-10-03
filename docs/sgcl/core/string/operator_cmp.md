[sgcl](../../README.md) › [core](../README.md) › [string](README.md)

# sgcl::operator==, operator\<=\> (sgcl::string)

```cpp
namespace sgcl {
    template<class CharT, class Traits>
    bool operator==(const basic_string<CharT, Traits>& a,                               // (1)
                    const basic_string<CharT, Traits>& b) noexcept;
    template<class CharT, class Traits>
    std::strong_ordering operator<=>(const basic_string<CharT, Traits>& a,              // (2)
                                     const basic_string<CharT, Traits>& b) noexcept;
    template<class CharT, class Traits>
    bool operator==(const basic_string<CharT, Traits>& a,                               // (3)
                    const slice<const CharT>& b) noexcept;
    template<class CharT, class Traits>
    std::strong_ordering operator<=>(const basic_string<CharT, Traits>& a,              // (4)
                                     const slice<const CharT>& b) noexcept;
}
```

Compare two strings, or a string and a text slice, by their characters.

1. Checks whether `a` and `b` hold the same characters, in four steps, each deciding when it can: the same object
   (a copy) is equal; different lengths are unequal; when both strings have their hashes computed and the hashes
   differ, they are unequal, without a character read; the characters are compared last.
2. Orders `a` and `b` by their characters, lexicographically, as `std::basic_string_view` orders them: a `char` is
   compared as an `unsigned char`, so the bytes of UTF-8 order the texts by their code points.
3. Checks whether `a` holds the characters of the slice `b`.
4. Orders `a` and the slice `b` by their characters, as (2).

- (1–4) `!=`, `<`, `<=`, `>` and `>=` are made of these by the rules of C++20, and so are the comparisons with the
  operands the other way round: `b == a` of a slice and a string is (3).

A string against a `std::basic_string_view`, a literal or a pointer compares by the friends of
[mixin::text](../mixin/text/README.md), which a text slice shares: `name == "alice"` reads the characters, makes no string.

## Parameters

| Parameter | Description |
|---|---|
| `a` | the string on the left |
| `b` | the string or the slice on the right |

## Return value

1. `true` when the characters are the same, `false` otherwise.
2. `std::strong_ordering::less`, `equal` or `greater`, as `a` comes before `b`, is equal to it, or comes after.
3. `true` when the characters are the same, `false` otherwise.
4. The order of `a` and `b`, as (2).

## Complexity

- (1) Constant when the two are the same object, differ in length, or have different hashes already computed; linear
  in the length otherwise.
- (2–4) Linear in the length of the shorter.

## Exceptions

None.

## Notes

The equality uses a hash only when it is there: a hash is computed when something asks for it ([hash](hash.md), a
hash container), and `==` never computes one.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string name = "alice";
    string copy = name;
    string other("alice");
    println("{} {} {}", copy == name, other == name, other != string("bob"));
    println("{} {} {}", string("apple") < string("banana"), (string("b") <=> string("a")) > 0,
            string("é") > string("z"));

    string text = "alice and bob";
    string_slice first = text.as_slice(0, 5);
    println("{} {} {}", name == first, first == name, name < text.as_slice(10));
    println("{}", name == "alice");
}
```

Output:

```text
true true true
true true true
true true true
true
```

## See also

- [hash](hash.md): the hash of the characters, kept in the object
- [object](object.md): the identity of a string
- [sgcl::string](README.md)
