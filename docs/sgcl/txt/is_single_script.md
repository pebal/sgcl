[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::is_single_script

```cpp
#include "sgcl/txt/identifier.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    bool is_single_script(const string& text) noexcept;
}
```

Checks whether the text is written in one script, `Common` and `Inherited` aside: the digits, the punctuation and the
marks belong to every script and so do not make a second one. This is the question
[UTS #39](https://www.unicode.org/reports/tr39/) §5.1 asks, and it is the one that catches a name half in Latin and
half in Cyrillic, which is how nearly every attack on a name is built. A text of no script at all, an empty one or
one of digits, is single script.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text, UTF-8 |

## Return value

`true` when the text has code points of one script at most.

## Complexity

Linear in the length of the text.

## Exceptions

None.

## Notes

- **Script_Extensions is not used.** This asks the `Script` property ([script](script.md)). A code point that is
  `Common` although only two scripts use it, the Japanese prolonged sound mark `ー` being the one everybody meets,
  counts here as belonging to all of them. That makes the answer **more generous** than the specification's and
  never less, so a text this calls single script may be two by `Script_Extensions`.
- The scripts of a text are counted into **eight words of stack** rather than into a container. A name is written in
  one script or two, and the widest set UTS #39 names is four (Latin, Han, Hiragana and Katakana, which is
  Japanese), so eight is twice what anything a caller would call a name reaches. A ninth distinct script is not
  dropped: it stops the walk, and nine scripts are not one.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    for (auto s : {"paypal", "раypal", "расчёт", "ラーメン", "abc123", "変数abc"}) {
        println("{}: {}", s, txt::is_single_script(s));
    }
}
```

Output:

```text
paypal: true
раypal: false
расчёт: true
ラーメン: true
abc123: true
変数abc: false
```

## See also

- [restriction_level_of](restriction_level_of.md): the ladder the scripts of a name stand on
- [script](script.md): the script of a code point
- [txt](README.md)
