[sgcl](../../README.md) › [encoding](../README.md) › [ascii85](../ascii85.md) › [encoder](../ascii85-encoder.md)

# sgcl::encoding::ascii85::encoder::write, async_write

```cpp
/*(1)*/ expected<size_t, io::error> write(const slice<const byte>& data) const;
/*(2)*/ async::task<expected<size_t, io::error>> async_write(const slice<const byte>& data) const
            noexcept;
```

Encodes `data` into the writer under the encoder: the whole groups of four bytes go out at once, through the
encoder's block of 8 KB, and the bytes short of a group wait in the encoder for the next write or for
[close](close.md). What is written so is one text, whatever the pieces: two writes of `"hello, "` and `"world"`
write the text of `"hello, world"`.

1. Waits on this thread as the writer under it does.
2. The same in a task, `co_await a85.async_write(data)`, over the writer's `async_write`.

A failure of the writer under it is kept for good: this write and every later `write` and `close` report it. A
write after `close()` is `io::errc::closed`. The text and the byte of the writers of the library,
`a85.write("text")`, are [io::mixin::writer](../../io/mixin/writer.md)'s, through this one.

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
    encoding::ascii85::encoder a85 = encoding::ascii85::encoder_to(out);
    println("{}", *a85.write("Hello"));
    println("'{}'", out.text());  // the fifth byte waits for its group
    a85.close();
    println("'{}'", out.text());
    println("{}", a85.write("x").error().message());
}
```

Output:

```text
5
'87cUR'
'87cURDZ'
write ascii85: stream closed
```

## See also

- [close, async_close](close.md): the last group
- [encode](../ascii85/encode.md): the text at once
- [sgcl::encoding::ascii85::encoder](../ascii85-encoder.md)
