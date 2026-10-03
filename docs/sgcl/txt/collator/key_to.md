[sgcl](../../README.md) › [txt](../README.md) › [collator](README.md)

# sgcl::txt::collator::key_to

```cpp
size_t key_to(const slice<byte>& buffer, const string& text) const noexcept;
```

Writes the sort key of a text, the one [key](key.md) returns, into a buffer the caller owns — the buffer first, as
in [format_to](../format_to.md). That is what a sort and an index builder want: there a key lives no longer than
the pass that uses it, and one allocation a word would be the whole cost.

When the key needs more than the buffer holds, **nothing is written** and the size still comes back: a truncated
key would compare as a different text, which is worse than none. So a caller may ask with an empty buffer first and
then size one, or simply try again with a larger one.

## Parameters

| Parameter | Description |
|---|---|
| `buffer` | where the key is written, from its first byte |
| `text` | the text, UTF-8 |

## Return value

The number of bytes the key takes, whether it was written or not: it was written when this is at most
`buffer.size()`.

## Complexity

Linear in the length of the text.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::collator c;
    array<byte, 8> small;
    size_t need = c.key_to(small, "żaba");
    println("needs {}, fits: {}", need, need <= small.size());

    vector<byte> buffer(need);
    println("wrote {}, same as key(): {}", c.key_to(buffer, "żaba"), buffer == c.key("żaba"));
}
```

Output:

```text
needs 40, fits: false
wrote 40, same as key(): true
```

## See also

- [key](key.md): the key as a new vector
- [sgcl::txt::collator](README.md)
