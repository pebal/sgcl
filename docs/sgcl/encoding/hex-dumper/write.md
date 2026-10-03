[sgcl](../../README.md) › [encoding](../README.md) › [hex](../hex/README.md) › [dumper](README.md)

# sgcl::encoding::hex::dumper::write, async_write

```cpp
expected<size_t, io::error> write(const slice<const byte>& data) const;                      // (1)
async::task<expected<size_t, io::error>> async_write(const slice<const byte>& data) const    // (2)
    noexcept;
```

Dumps `data` into the writer under the dumper: every line whose sixteen bytes are there goes out at once,
through the dumper's block of 8 KB, and the bytes short of a line wait in the dumper for the next write or for
[close](close.md). The offsets are counted across the writes, so the lines are those [dump](../hex/dump.md)
writes of everything written, whatever the pieces.

1. Waits on this thread as the writer under it does.
2. The same in a task, `co_await wire.async_write(data)`, over the writer's `async_write`.

A failure of the writer under it is kept for good: this write and every later `write` and `close` report it. A
write after `close()` is `io::errc::closed`. The text and the byte of the writers of the library,
`wire.write("text")`, are [io::mixin::writer](../../io/mixin/writer/README.md)'s, through this one.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes to dump |

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
    encoding::hex::dumper wire = encoding::hex::dumper_to(out);
    wire.write("0123456789");
    println("{} characters", out.text().size());  // ten bytes wait for their line
    wire.write("abcdefghij");
    print(out.text());
}
```

Output:

```text
0 characters
00000000  30 31 32 33 34 35 36 37  38 39 61 62 63 64 65 66  |0123456789abcdef|
```

## See also

- [close, async_close](close.md): the short line at the end
- [dump](../hex/dump.md): the dump at once
- [sgcl::encoding::hex::dumper](README.md)
