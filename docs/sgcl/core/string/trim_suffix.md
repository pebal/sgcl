[sgcl](../../README.md) › [core](../README.md) › [string](../string.md)

# sgcl::string::trim_suffix

```cpp
/*(1)*/ basic_string trim_suffix(view_type suffix) const noexcept;
/*(2)*/ template<size_t N>
        basic_string trim_suffix(const CharT (&suffix)[N]) const noexcept;
```

Returns the string without `suffix` at its end when it ends with it, and the same object when it does not, as Go's
`strings.TrimSuffix`. The suffix is taken off once, not as often as it repeats.

1. The suffix as a view: a string, a slice, a `std::string`, a pointer up to its NUL convert to it.
2. The suffix as an array, a literal, read up to its first NUL or its end, whichever comes first.

## Parameters

| Parameter | Description |
|---|---|
| `suffix` | the text to take off the end |

## Return value

A new string of the characters before `suffix`; this string's object when it does not end with `suffix` or `suffix`
is empty; the empty string when the string is `suffix` and nothing else.

## Complexity

Linear in the length of `suffix`, plus the copy of the rest when it was there.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string file = "report.tar.gz";
    println("{}", file.trim_suffix(".gz"));
    println("{}", file.trim_suffix(".zip").object() == file.object());
    println("{}", string(".gz").trim_suffix(".gz").empty());
}
```

Output:

```text
report.tar
true
true
```

## See also

- [trim_prefix](trim_prefix.md): without a prefix
- [trim_right](trim_right.md): without white space, or the characters given, at the end
- [mixin::text](../mixin/text.md): `ends_with`
- [sgcl::string](../string.md)
