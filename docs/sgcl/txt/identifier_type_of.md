[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::identifier_type_of

```cpp
#include "sgcl/txt/identifier.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    inline constexpr /* unspecified */ identifier_type_of {};   // called as identifier_type_of(c)

    template<class... T> requires (std::same_as<T, char32_t> && ...)
    constexpr auto operator()(T... c) const noexcept;                      // (1)
    template<class... T> requires (!(std::same_as<T, char32_t> && ...))
    constexpr auto operator()(T...) const noexcept = delete;               // (2)
}
```

An object called as a function, `txt::identifier_type_of(c)`, and passed as a projection. It takes a `char32_t` and
refuses everything else, as the [module's rules](README.md) say of every question about a code point.

1. Returns the `Identifier_Type` of the code point `c` by [UTS #39](https://www.unicode.org/reports/tr39/), an
   [identifier_type](identifier_type.md): what is wrong with it when it does not belong in a name (deprecated,
   technical, obsolete, an exclusion, a compatibility form), or that it is recommended or an inclusion. The answer is
   a **set** and not one value, read with `&`:
   `(identifier_type_of(c) & identifier_type::technical) != identifier_type::not_character`. A code point the file
   does not name is `not_character`, the empty set.
2. Every other argument is refused: a `char`, an `int`, a `char8_t`, a `char16_t`, a `wchar_t`.

## Parameters

| Parameter | Description |
|---|---|
| `c` | the code point |

## Return value

The set of the types of `c`, [identifier_type](identifier_type.md).

## Complexity

Constant: two reads of a table below U+10000, a binary search over sorted ranges above.

## Exceptions

None.

## Notes

`IdentifierType.txt` of UTS #39 is held **whole**: 1 682 ranges of `Identifier_Type`.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    using type = txt::identifier_type;
    println("{}", txt::identifier_type_of(U'a') == type::recommended);
    println("{}", txt::identifier_type_of(U'ſ') == type::not_nfkc);
    auto t = txt::identifier_type_of(U'ǀ');  // a click letter
    println("{}", (t & type::technical) != type::not_character);
}
```

Output:

```text
true
true
true
```

## See also

- [identifier_type](identifier_type.md): the types
- [identifier_status_of](identifier_status_of.md): whether the code point is allowed
- [txt](README.md)
