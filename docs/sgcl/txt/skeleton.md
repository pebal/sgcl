[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::skeleton

```cpp
#include "sgcl/txt/identifier.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    string skeleton(const string& text);
}
```

Returns the skeleton of the text by [UTS #39](https://www.unicode.org/reports/tr39/) §4: the text decomposed, every
code point replaced by the prototype it is confusable with, decomposed again. Two texts a reader could mistake for one
another have the same skeleton: `"раypal"` written with a Cyrillic `а` and `р` has the skeleton of `"paypal"`, and so
does `"paypaI"` with a capital i for the l; `"m"` has the skeleton `"rn"`.

**A skeleton is not text.** It is a key to compare by and to look up in a table of the names already taken; it is
not to be shown, and no reader should ever see one. A text that is its own skeleton comes back as the same object.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text, UTF-8; an invalid byte is read as `U+FFFD` |

## Return value

The skeleton of the text; `text` itself, the same object, when it is its own skeleton.

## Complexity

Linear in the length of the text.

## Exceptions

`length_error` when the result would pass the [max_size()](../core/string/max_size.md) of a string.

## Notes

- **The confusables table is one judgement, in one font.** `"rn"` for `"m"` is in it, `"1"` for `"l"` is,
  `"paypa1"` is caught. Whether two glyphs look alike on the reader's screen, in the reader's font, at the reader's
  size is not a question any table answers.
- The oracle is the confusables file of UTS #39, held **whole**: 6 355 prototypes. The second decomposition of the
  three steps is folded into the mapping: a code point the table does not name came out of the first one decomposed
  already, and a prototype is taken apart where it is written rather than on a pass of its own.
- The confusables are the largest table of the identifiers, 59.4 KB, and come into a program only with `skeleton`.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    set<string> taken;
    for (auto name : {"paypal", "wartość"}) {
        taken.insert(txt::skeleton(name));
    }
    for (auto wanted : {"раypal", "paypaI", "wartos\u0301c\u0301", "wartosc"}) {
        println("{}: {}", wanted, taken.contains(txt::skeleton(wanted)) ? "taken" : "free");
    }
}
```

Output:

```text
раypal: taken
paypaI: taken
wartość: taken
wartosc: free
```

## See also

- [is_confusable](is_confusable.md): two texts compared by their skeletons
- [nfkc_casefold](nfkc_casefold.md): the form names are compared in
- [txt](README.md)
