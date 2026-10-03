[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::identifier_status_of

```cpp
#include "sgcl/txt/identifier.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    // called as identifier_status_of(c)
    inline constexpr /* unspecified */ identifier_status_of {};

    /*(1)*/ template<class... T> requires (std::same_as<T, char32_t> && ...)
            constexpr auto operator()(T... c) const noexcept;
    /*(2)*/ template<class... T> requires (!(std::same_as<T, char32_t> && ...))
            constexpr auto operator()(T...) const noexcept = delete;
}
```

An object called as a function, `txt::identifier_status_of(c)`, and passed as a projection. It takes a `char32_t`
and refuses everything else, as the [module's rules](README.md) say of every question about a code point.

1. Returns the `Identifier_Status` of the code point `c` by [UTS #39](https://www.unicode.org/reports/tr39/), an
   [identifier_status](identifier_status.md): whether it is one the specification would let into a name at all.
   `restricted` is the default: a code point nobody has argued for is not allowed.
2. Every other argument is refused: a `char`, an `int`, a `char8_t`, a `char16_t`, a `wchar_t`.

## Parameters

| Parameter | Description |
|---|---|
| `c` | the code point |

## Return value

`identifier_status::allowed` or `identifier_status::restricted`.

## Complexity

Constant: two reads of a table below U+10000, a binary search over sorted ranges above.

## Exceptions

None.

## Notes

- `IdentifierStatus.txt` of UTS #39 is held **whole**: 391 ranges of `Allowed` over 112 778 code points, with Table 1
  of the specification checked as a property, `Allowed` is exactly `Recommended` or `Inclusion`
  ([identifier_type_of](identifier_type_of.md)), over the whole space.
- **The tables are Unicode 16.0.0.** A code point assigned tomorrow is `not_character` and `restricted` today, which
  is the safe way round and still a difference.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    string name = "żółw_ſ";
    int restricted = 0;
    for (char32_t c : name.runes()) {
        restricted += txt::identifier_status_of(c) == txt::identifier_status::restricted;
    }
    println("{} restricted", restricted);
}
```

Output:

```text
1 restricted
```

## See also

- [identifier_status](identifier_status.md): the two statuses
- [is_allowed_identifier](is_allowed_identifier.md): every code point of a text
- [identifier_type_of](identifier_type_of.md): why a code point is restricted
- [txt](README.md)
