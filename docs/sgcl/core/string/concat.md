[sgcl](../../README.md) › [core](../README.md) › [string](README.md)

# sgcl::string::concat

```cpp
template<class... A>
requires (sizeof...(A) > 0)
      && ((std::is_convertible_v<const A&, view_type> || std::is_same_v<A, CharT>) && ...)
static basic_string concat(const A&... pieces);
```

Makes one string of the pieces, in order. A static function: `string::concat(user, '@', host)`. A piece is anything a
`view_type` is made of (a string, a slice, a `std::string_view`, a `std::string`, a literal) or a character, a
`CharT`; an array, a literal, is read up to its first NUL or its end, whichever comes first. At least one piece is
given.

The lengths of the pieces are summed first, and each piece is written once into a string of that size: one
allocation. It is the way to build a text of a few known parts, where `a + b + c` makes a string of its own for each
`+` and reads the characters of the first ones again.

## Parameters

| Parameter | Description |
|---|---|
| `pieces` | the pieces of the string, in order |

## Return value

The new string of the pieces; the empty string when every piece is empty.

## Complexity

Linear in the length of the result.

## Exceptions

`length_error` when the result would pass `max_size()`.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <string>

using namespace sgcl;

int main() {
    string user = "alice";
    std::string host = "example.com";
    string address = string::concat(user, '@', host);
    println("{}", address);

    string_slice name = address.as_slice(0, 5);
    println("{}", string::concat("<", name, std::string_view("> "), address));
    println("{}", string::concat("", string()).empty());
}
```

Output:

```text
alice@example.com
<alice> alice@example.com
true
```

## See also

- [join](join.md): one string of a range of parts with a separator between each two
- [repeat](repeat.md): the string a number of times over
- [operator+](operator_arith.md): a new string of two texts
- [sgcl::string](README.md)
