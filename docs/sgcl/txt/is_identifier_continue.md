[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::is_identifier_continue

```cpp
#include "sgcl/txt/identifier.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    // called as is_identifier_continue(c)
    inline constexpr /* unspecified */ is_identifier_continue {};

    /*(1)*/ constexpr bool operator()(char32_t c) const noexcept;
    /*(2)*/ constexpr bool operator()(char32_t c, program_syntax_t) const noexcept;
    /*(3)*/ template<class T, class... Rest> requires (!std::same_as<T, char32_t>)
            constexpr bool operator()(T, Rest...) const noexcept = delete;
}
```

An object called as a function, `txt::is_identifier_continue(c)`, and passed as a predicate:
`s.runes().all(txt::is_identifier_continue)`. It takes a `char32_t` and refuses everything else, as the
[module's rules](README.md) say of every question about a code point.

1. Checks whether the code point `c` may stand in an identifier after its first code point: the `XID_Continue`
   property of [UAX #31](https://www.unicode.org/reports/tr31/), which is `XID_Start` with the digits, the marks and
   the connector punctuation, `_` among them. The X is the closure under normalization
   ([is_identifier_start](is_identifier_start.md)).
2. The same with the [program_syntax](program_syntax_t.md) profile: `$` too.
3. Every other first argument is refused: a `char`, an `int`, a `char8_t`, a `char16_t`, a `wchar_t`.

The two joiners, `U+200C` and `U+200D`, are in neither set; [is_identifier](is_identifier.md) lets them in where rule
R1a does.

## Parameters

| Parameter | Description |
|---|---|
| `c` | the code point |

## Return value

`true` when `c` may stand in an identifier after its start.

## Complexity

Constant: two reads of a table below U+10000, a binary search over sorted ranges above.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    string name = "wartość_2";
    println("{}", name.runes().all(txt::is_identifier_continue));
    println("{} {}", txt::is_identifier_continue(U'$'),
            txt::is_identifier_continue(U'$', txt::program_syntax));
    println("{} {}", txt::is_identifier_continue(U'\u0301'), txt::is_identifier_continue(U'-'));
}
```

Output:

```text
true
false true
true false
```

## See also

- [is_identifier_start](is_identifier_start.md): whether a code point may begin an identifier
- [is_identifier](is_identifier.md): a whole text
- [txt](README.md)
