[sgcl](../../README.md) › [txt](../README.md) › [match](README.md)

# sgcl::txt::match::text

```cpp
slice<const char> text() const noexcept;
```

Returns the whole match as a slice of the text it was found in: the bytes from [begin_at](begin_at.md) to
[end_at](end_at.md). It is group 0, and [group](group.md)`(0)` gives the same.

## Parameters

None.

## Return value

The bytes of the match; an empty slice for a match of no width.

## Complexity

Constant.

## Exceptions

None.

## Notes

The slice holds the text's object: it stays valid after the match, the regex and the string are gone.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    auto m = txt::regex("\\p{Lu}\\w+").find("wczoraj Łódź, dziś Kraków");
    slice<const char> word = m->text();
    println("{} ({} bytes)", word, word.size());
}
```

Output:

```text
Łódź (7 bytes)
```

## See also

- [group](group.md): a group of the match
- [subject](subject.md): the whole text
- [sgcl::txt::match](README.md)
