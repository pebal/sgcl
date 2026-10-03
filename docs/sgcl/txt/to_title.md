[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::to_title

```cpp
#include "sgcl/txt/case.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    string to_title(const string& text, locale where = {});
}
```

Returns `text` with the first cased letter of every word in title case and the rest of the word in lower case,
by the full mappings and the language `where`. The words are the ones UAX #29 finds ([word_breaks](word_breaks.md)),
so `"don't"` is one word and its apostrophe does not start a new one. Title case is not upper case for the letters
that are two: `ǆ` is `ǅ` at the start of a word.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text, UTF-8 |
| `where` | the language; the root locale by default |

## Return value

The text in title case.

## Complexity

Linear in the length of the text.

## Exceptions

`length_error` when the result would pass a string's `max_size()`.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    println("{}", txt::to_title("don't stop me now"));
    println("{}", txt::to_title("ǆungla STRASSE"));
    println("{}", txt::to_title("istanbul", txt::locale::turkish()));
}
```

Output:

```text
Don't Stop Me Now
ǅungla Strasse
İstanbul
```

## See also

- [to_lower_full](to_lower_full.md), [to_upper_full](to_upper_full.md)
- [word_breaks](word_breaks.md): the words
- [sgcl::txt](README.md)
