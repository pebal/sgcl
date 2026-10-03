[sgcl](../../README.md) › [txt](../README.md) › [match](../match.md)

# sgcl::txt::match::begin_at

```cpp
size_t begin_at() const noexcept;
```

Returns the byte position in the text where the match begins: Python's `m.start()`. A position is a byte, not a
code point, because a slice of a text is bytes of it.

## Parameters

None.

## Return value

The position of the first byte of the match; for a match of no width, the place it stands.

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
    string text = "żółw i kot";
    auto m = txt::regex("kot").find(text);
    println("{} {}", m->begin_at(), text.as_slice(0, m->begin_at()));
}
```

Output:

```text
10 żółw i 
```

## See also

- [end_at](end_at.md): the position past the end
- [sgcl::txt::match](../match.md)
