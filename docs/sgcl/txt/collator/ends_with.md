[sgcl](../../README.md) › [txt](../README.md) › [collator](README.md)

# sgcl::txt::collator::ends_with

```cpp
bool ends_with(const string& text, const string& pattern) const noexcept;
```

Checks whether the text ends with the pattern, by the same equality and the same boundaries as [find](find.md),
asked at the end of the text: with nothing behind the match that this collator looks at. A match that ends there
takes the last elements the collator looks at and no others, so there is a single element it can begin at, found by
a walk back over the pattern's length.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text, UTF-8 |
| `pattern` | the ending to look for |

## Return value

`true` when the text ends with the pattern, `false` otherwise; `true` for an empty pattern, `false` for one the
collator does not look at.

## Complexity

Linear in the length of the text for the weighing; the match is tried at one place.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::collator primary{txt::strength::primary};
    for (const char* file : {"ZDJĘCIE.JPG", "zdjecie.jpg", "zdjęcie.jpeg"}) {
        print("{} ", primary.ends_with(file, ".jpg"));
    }
    println();
}
```

Output:

```text
true true false 
```

## See also

- [starts_with](starts_with.md): the other end
- [sgcl::txt::collator](README.md)
