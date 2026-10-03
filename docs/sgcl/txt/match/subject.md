[sgcl](../../README.md) › [txt](../README.md) › [match](../match.md)

# sgcl::txt::match::subject

```cpp
const slice<const char>& subject() const noexcept;
```

Returns the whole text the match was found in, as a slice of it: Python's `m.string`. The positions of the match
and of its groups are offsets in it.

## Parameters

None.

## Return value

The text the search was given; for a C text, the copy the match holds.

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
    auto m = txt::regex("ma").find("Ala ma kota");
    auto all = m->subject();
    println("[{}][{}][{}]", all.subslice(0, m->begin_at()), m->text(), all.subslice(m->end_at()));
}
```

Output:

```text
[Ala ][ma][ kota]
```

## See also

- [text](text.md): the bytes of the match
- [sgcl::txt::match](../match.md)
