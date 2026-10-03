[sgcl](../../README.md) › [txt](../README.md) › [collator](../collator.md)

# sgcl::txt::collator::starts_with

```cpp
bool starts_with(const string& text, const string& pattern) const noexcept;
```

Checks whether the text begins with the pattern, by the same equality and the same boundaries as [find](find.md),
asked at one end of the text rather than everywhere in it. At the beginning means with nothing in front of it that
this collator looks at — not at byte zero, because that is a question about the bytes and this is a search by
collation: with the punctuation shifted a hyphen is not there to be found around, and it is not there at the ends
either, so `"-resume"` starts with `"resume"` as it contains it.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text, UTF-8 |
| `pattern` | the beginning to look for |

## Return value

`true` when the text begins with the pattern, `false` otherwise; `true` for an empty pattern, `false` for one the
collator does not look at.

## Complexity

Linear in the length of the text for the weighing; the match is tried at the first elements alone.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::collator primary{txt::strength::primary};
    txt::collator shifted{{.strength = txt::strength::primary,
                           .punctuation = txt::punctuation::shifted}};
    println("{} {}", primary.starts_with("Ósemka", "osem"),
            primary.starts_with("-resume", "resume"));
    println("{}", shifted.starts_with("-resume", "resume"));
}
```

Output:

```text
true false
true
```

## See also

- [ends_with](ends_with.md): the other end
- [find](find.md): anywhere in the text
- [sgcl::txt::collator](../collator.md)
