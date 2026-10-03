[sgcl](../../README.md) › [encoding](../README.md) › [hex](../hex.md) › [encoder](../hex-encoder.md)

# sgcl::encoding::hex::encoder::write, async_write

```cpp
/*(1)*/ expected<size_t, io::error> write(const slice<const byte>& data) const;
/*(2)*/ async::task<expected<size_t, io::error>> async_write(const slice<const byte>& data) const
            noexcept;
```

Writes the lower-case digits of `data` to the writer under the encoder, through the encoder's block of 8 KB. A
byte is a whole group: every digit of `data` is written before the call returns, and nothing waits for
[close](close.md).

1. Waits on this thread as the writer under it does.
2. The same in a task, `co_await digits.async_write(data)`, over the writer's `async_write`.

A failure of the writer under it is kept for good: this write and every later `write` and `close` report it. A
write after `close()` is `io::errc::closed`. The text and the byte of the writers of the library,
`digits.write("text")`, are [io::mixin::writer](../../io/mixin/writer.md)'s, through this one.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes to encode |

## Return value

`data.size()`, or the `io::error` of the writer under it, or `io::errc::closed`.

## Complexity

Linear in the size of `data`.

## Exceptions

- (1) What the write of the writer under it throws; the writers of the library throw nothing.
- (2) None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer out;
    encoding::hex::encoder digits = encoding::hex::encoder_to(out);
    vector<byte> header = {byte(0xFF), byte(0xD8), byte(0xFF), byte(0xE0)};
    println("{}", *digits.write(header));
    println("{}", out.text());
    digits.close();
    println("{}", digits.write("x").error().message());
}
```

Output:

```text
4
ffd8ffe0
write hex: stream closed
```

## See also

- [close, async_close](close.md): the end of the encoder
- [encode](../hex/encode.md): the digits at once
- [sgcl::encoding::hex::encoder](../hex-encoder.md)
