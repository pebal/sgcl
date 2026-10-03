[sgcl](../../README.md) › [txt](../README.md) › [match](../match.md)

# sgcl::txt::match::match

```cpp
match() noexcept = default;
```

Constructs an empty match of no text: [text](text.md) and [subject](subject.md) are empty slices,
[begin_at](begin_at.md) and [end_at](end_at.md) are 0, and it has no groups. A match of a text is made by
[regex::find](../regex/find.md) and the range of [regex::all](../regex/all.md); this one is the value a variable
holds before one is assigned to it.

## Parameters

None.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::match m;
    println("{} {} {}", m.empty(), m.group_count(), m.text().size());
    m = *txt::regex("\\d+").find("abc 123");
    println("{}", m.text());
}
```

Output:

```text
true 0 0
123
```

## See also

- [regex::find](../regex/find.md): the first match of a text
- [sgcl::txt::match](../match.md)
