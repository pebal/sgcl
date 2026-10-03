[sgcl](../../README.md) › [encoding](../README.md) › [ascii85](../ascii85/README.md) › [decoder](README.md)

# sgcl::encoding::ascii85::decoder::read, async_read

```cpp
expected<size_t, io::error> read(const slice<byte>& buffer) const;                      // (1)
async::task<expected<size_t, io::error>> async_read(const slice<byte>& buffer) const    // (2)
    noexcept;
```

Reads the next bytes of the decoding into `buffer`: the groups the text read so far holds, decoded straight into
the buffer, and when there are none, more text from the reader under the decoder into its block of 8 KB. A group
cut between two reads of the text waits for the rest, so the text may come in pieces of any size; a buffer
smaller than a group gets the group's bytes through the decoder over the reads that follow. At the end of the
text, the end of the decoding: `0`, after the short last group.

1. Waits on this thread as the reader under it does.
2. The same in a task, `co_await plain.async_read(buffer)`, over the reader's `async_read`.

A text that is not Ascii85 fails the read that reaches the error, after the bytes before it were handed out, and
every read after. The `io::error` has the [errc](../errc.md) code in the `encoding` category, and
[last_error](last_error.md) holds the [error](../error/README.md) with its offset in the text. A failure of the reader
under it is the read's failure too, kept for good.

## Parameters

| Parameter | Description |
|---|---|
| `buffer` | where the bytes go |

## Return value

The number of bytes read, `0` at the end of the decoding or for an empty `buffer`, or the `io::error`.

## Complexity

Linear in the bytes read and the text read for them.

## Exceptions

- (1) What the read of the reader under it throws; the readers of the library throw nothing.
- (2) None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer text;
    text.write("87cURD~");
    encoding::ascii85::decoder plain = encoding::ascii85::decoder_from(text);
    array<byte, 16> buffer;
    for (;;) {
        auto n = plain.read(buffer);
        if (!n) {
            println("{}", n.error().message());
            println("{}", plain.last_error()->message());
            break;
        }
        println("{} bytes", *n);
    }
}
```

Output:

```text
4 bytes
decode ascii85: invalid character
offset 6: invalid character '~'
```

## See also

- [last_error](last_error.md): where the text went wrong
- [decode](../ascii85/decode.md): the bytes at once
- [sgcl::encoding::ascii85::decoder](README.md)
