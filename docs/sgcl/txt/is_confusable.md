[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::is_confusable

```cpp
#include "sgcl/txt/identifier.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    bool is_confusable(const string& a, const string& b);
}
```

Checks whether two texts look alike by [UTS #39](https://www.unicode.org/reports/tr39/): whether they are equal or
have the same [skeleton](skeleton.md). What this catches is the whole of what the table of UTS #39 knows and nothing
more.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the texts, UTF-8 |

## Return value

`true` when `a == b` or `skeleton(a) == skeleton(b)`.

## Complexity

Linear in the lengths of the two texts.

## Exceptions

`length_error` when a skeleton would pass the [max_size()](../core/string/max_size.md) of a string.

## Notes

- **The confusables table is one judgement, in one font.** `"rn"` for `"m"` is in it, `"1"` for `"l"` is,
  `"paypa1"` is caught. Whether two glyphs look alike on the reader's screen, in the reader's font, at the reader's
  size is not a question any table answers.
- **A similar name is not a confusable one.** `"paypal-inc"` against `"paypal"` is a different name, not the same
  one written differently, and nothing here will say a word about it. Whole-name similarity, edit distance and the
  domains people mistype are a different problem.
- A registry that keeps the names already taken keeps their skeletons and asks [skeleton](skeleton.md) of a new name
  once, rather than comparing it with every name in turn.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    string wanted = "раypal";  // the а and the р are Cyrillic
    string taken = "paypal";
    bool folded = txt::nfkc_casefold(wanted) == txt::nfkc_casefold(taken);
    println("same bytes {}, same folded {}, confusable {}", wanted == taken, folded,
            txt::is_confusable(wanted, taken));
    println("{} {} {}", txt::is_confusable("rn", "m"), txt::is_confusable("paypa1", "paypal"),
            txt::is_confusable("paypal-inc", "paypal"));
}
```

Output:

```text
same bytes false, same folded false, confusable true
true true false
```

## See also

- [skeleton](skeleton.md): the key the texts are compared by
- [restriction_level_of](restriction_level_of.md): the scripts a name mixes
- [txt](README.md)
