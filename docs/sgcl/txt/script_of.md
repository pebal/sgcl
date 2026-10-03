[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::script_of

```cpp
#include "sgcl/txt/properties.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    inline constexpr /* unspecified */ script_of {};   // called as script_of(c)

    /*(1)*/ template<class... T> requires (std::same_as<T, char32_t> && ...)
            constexpr auto operator()(T... c) const noexcept;
    /*(2)*/ template<class... T> requires (!(std::same_as<T, char32_t> && ...))
            constexpr auto operator()(T...) const noexcept = delete;
}
```

An object called as a function, `txt::script_of(c)`, and passed as a projection. It takes a `char32_t` and refuses everything else, as the [module's rules](README.md) say of every question about a code point.

1. Returns the script the code point `c` is written in, a [script](script.md): `script::common` for what many scripts share (the digits, the punctuation, the space), `script::inherited` for a combining mark, which takes the script of what it sits on, and `script::unknown` for a code point of none.
2. Every other argument is refused: a `char` (a byte of UTF-8), an `int` (a multi-character literal), a `char8_t`, a `char16_t`, a `wchar_t`.

## Parameters

| Parameter | Description |
|---|---|
| `c` | the code point |

## Return value

The script of `c`.

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
    string digits = "٣ ١ ٤";
    bool arabic = digits.runes().exists([](char32_t c) {
        return txt::script_of(c) == txt::script::arabic;
    });
    println("{} {}", arabic, txt::script_of(U'1') == txt::script::common);
}
```

Output:

```text
true true
```

## See also

- [script](script.md)
- [is_single_script](is_single_script.md): the scripts of a name
- [sgcl::txt](README.md)
