[sgcl](../../README.md) › [compress](../README.md) › [bzip2](../bzip2/README.md) › [reader](README.md)

# sgcl::compress::bzip2::reader::last_error

```cpp
const optional<error>& last_error() const noexcept;
```

Returns the error of the data, kept: the [compress::error](../error/README.md) whose code, offset and detail a read gave as
an `io::error` of the compress category. The read that reached it and every read after give it. A failure of `in`
itself is not kept here: the read returns `in`'s error as it came.

## Parameters

None.

## Return value

The error of the data, or `nullopt` while there was none.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto packed = encoding::hex::decode("425a68393141592653599c453ed3000003910040040244a0002122"
                                        "3030065227e454585dc914e142427114fb4c");
    (*packed)[20] ^= byte(1);  // damaged on the way

    compress::bzip2::reader r{io::buffer(*packed)};
    auto text = r.read_all_text();
    if (!text) {
        println("{}", text.error().message());
        println("{}", r.last_error()->message());
    }
}
```

Output:

```text
read bzip2: corrupt data
offset 33: bzip2: original pointer outside the block
```

## See also

- [compress::error](../error/README.md)
- [sgcl::compress::bzip2::reader](README.md)
