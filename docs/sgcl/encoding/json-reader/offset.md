[sgcl](../../README.md) › [encoding](../README.md) › [json](../json.md) › [reader](../json-reader.md)

# sgcl::encoding::json::reader::offset

```cpp
uint64_t offset() const noexcept;
```

The byte of the input the reader is at: where the next token starts, or the white space before it. It counts
from the start of the whole input, across the blocks of a stream the reader let go, so a loader can say where
each piece of its input was. After a token, it is the byte just past the token. Go's `Decoder.InputOffset`.

## Parameters

None.

## Return value

The offset of the next byte the reader looks at, from 0.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::json::reader r(string(R"([10, "ab", null])"));
    size_t from = r.offset();
    while (auto t = r.next()) {
        println("{}-{} {}", from, r.offset(), t->text());
        from = r.offset();
    }
}
```

Output:

```text
0-1 [
1-3 10
3-9 ab
9-15 null
15-16 ]
```

## See also

- [depth](depth.md): the arrays and objects open
- [last_error](last_error.md): the offset of an error
- [sgcl::encoding::json::reader](../json-reader.md)
