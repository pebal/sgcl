[sgcl](../../README.md) › [txt](../README.md) › [regex](../regex.md)

# sgcl::txt::regex::replace

```cpp
/*(1)*/ string replace(const slice<const char>& text, const slice<const char>& with) const;
/*(2)*/ string replace(const string& text, const string& with) const;
/*(3)*/ template<size_t N, size_t M>
        string replace(const char (&text)[N], const char (&with)[M]) const;
/*(4)*/ template<class P, class Q>
        requires (std::same_as<P, const char*> || std::same_as<P, char*>)
              && (std::same_as<Q, const char*> || std::same_as<Q, char*>)
        string replace(const P& text, const Q& with) const;
```

Returns the text with every match of the pattern replaced, the matches never overlapping. In the replacement:

| Written | Stands for |
|---|---|
| `$1`, `$12` | what that group matched |
| `${1}`, `${name}` | what that group, by number or by name, matched |
| `$0` | the whole match |
| `$$` | a dollar |

A group the pattern does not have, and a group that took no part in the match, put nothing in, as in Go: a
replacement is written by a person, and a missing group is far more often a typing mistake than a request for the
text `$7`. A `$` before anything else stands for itself. A match of no width is replaced too, and the search moves
on by one code point, which is copied.

1. The text and the replacement as slices.
2. The text and the replacement as strings.
3. Two arrays of `char`, each up to its first NUL or its end.
4. Two pointers, each up to its NUL. An array beside a pointer goes through `string`, (2).

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text, UTF-8 |
| `with` | the replacement, with `$` references to the groups |

## Return value

A new string: the text between the matches as it was, each match replaced.

## Complexity

Linear in the length of the text times the length of the pattern, and in the length of the result.

## Exceptions

`length_error` when the result is longer than 4294967295 bytes, the most a [string](../../core/string.md) holds.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::regex date("(?<d>\\d{2})\\.(?<m>\\d{2})\\.(?<y>\\d{4})");
    println("{}", date.replace("od 01.10.2026 do 15.12.2026", "${y}-$2-$1"));
    println("{}", txt::regex("(\\w+)@").replace("jan@ i ola@", "$$$1 $9|"));
    println("{}", txt::regex("x*").replace("aż", "-"));
}
```

Output:

```text
od 2026-10-01 do 2026-12-15
$jan | i $ola |
-a-ż-
```

## See also

- [replace_first](replace_first.md): only the first match replaced
- [split](split.md): the pieces between the matches
- [sgcl::txt::regex](../regex.md)
