[sgcl](../../README.md) › [compress](../README.md) › [error](README.md)

# sgcl::compress::error::message

```cpp
string message() const noexcept;
```

Returns the error as a sentence: `offset N: `, then the detail the format gave or the code's own words
(`"checksum mismatch"`), then, when a stream failed, `: ` and the stream's message —
`"offset 20: gzip: CRC-32 mismatch"`, `"offset 0: zip: entry a/b.txt: CRC-32 mismatch"`,
`"offset 512: input/output error: read: ..."`. An error that did not come from the data — a file that does not
open, cannot be made, written or closed, a failure of what a writer writes into, a mistake of the calls to a
writer, a name the archive does not hold, a limit the caller set on what is extracted — has no place: its message
is the words alone, `"zip: no entry none.txt"`, `"input/output error: open missing.zip: No such file or
directory"`.

## Parameters

None.

## Return value

The sentence, a new string.

## Complexity

Linear in the length of the sentence.

## Exceptions

None.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto back = compress::zlib::decompress(compress::flate::compress("not zlib"));
    println("{}", back.error().message());
}
```

Output:

```text
offset 0: zlib: not a deflate stream with a window of 32 KB or less
```

## See also

- [code](code.md), [offset](offset.md), [io_error](io_error.md): the parts of it
- [sgcl::compress::error](README.md)
