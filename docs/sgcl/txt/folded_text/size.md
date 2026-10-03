[sgcl](../../README.md) › [txt](../README.md) › [folded_text](README.md)

# sgcl::txt::folded_text::size

```cpp
size_t size() const noexcept;
```

Returns the number of code points the text mapped to: neither its bytes nor its characters, since folding turns a
`"ß"` into two code points and decomposing turns an `"é"` of one into two.

## Parameters

None.

## Return value

The number of mapped code points.

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
    string word = "Straße";
    println("{} bytes, {} folded, {} decomposed", word.size(), txt::folded_text(word).size(),
            txt::normalized_text(word).size());
}
```

Output:

```text
7 bytes, 7 folded, 6 decomposed
```

## See also

- [empty](empty.md): whether the text mapped to nothing
- [sgcl::txt::folded_text, normalized_text](README.md)
