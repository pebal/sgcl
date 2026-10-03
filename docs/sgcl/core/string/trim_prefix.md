[sgcl](../../README.md) › [core](../README.md) › [string](../string.md)

# sgcl::string::trim_prefix

```cpp
basic_string trim_prefix(view_type prefix) const noexcept;            // (1)
template<size_t N>
basic_string trim_prefix(const CharT (&prefix)[N]) const noexcept;    // (2)
```

Returns the string without `prefix` at its start when it begins with it, and the same object when it does not, as
Go's `strings.TrimPrefix`. The prefix is taken off once, not as often as it repeats.

1. The prefix as a view: a string, a slice, a `std::string`, a pointer up to its NUL convert to it.
2. The prefix as an array, a literal, read up to its first NUL or its end, whichever comes first.

## Parameters

| Parameter | Description |
|---|---|
| `prefix` | the text to take off the start |

## Return value

A new string of the characters after `prefix`; this string's object when it does not begin with `prefix` or
`prefix` is empty; the empty string when the string is `prefix` and nothing else.

## Complexity

Linear in the length of `prefix`, plus the copy of the rest when it was there.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string url = "https://example.com";
    println("{}", url.trim_prefix("https://"));
    println("{}", url.trim_prefix("ftp://").object() == url.object());
    println("{}", string("aaab").trim_prefix("a"));
}
```

Output:

```text
example.com
true
aab
```

## See also

- [trim_suffix](trim_suffix.md): without a suffix
- [trim_left](trim_left.md): without white space, or the characters given, at the start
- [mixin::text](../mixin/text.md): `starts_with`
- [sgcl::string](../string.md)
