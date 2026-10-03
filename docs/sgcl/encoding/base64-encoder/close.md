[sgcl](../../README.md) › [encoding](../README.md) › [base64](../base64.md) › [encoder](../base64-encoder.md)

# sgcl::encoding::base64::encoder::close, async_close

```cpp
expected<void, io::error> close() const;                                // (1)
async::task<expected<void, io::error>> async_close() const noexcept;    // (2)
```

Writes the last group, the bytes that waited for one, with its padding when the codec is padded, and ends the
encoder: a write after it is `io::errc::closed`. The writer under it stays open, as Go's encoder leaves it, since
what is written around the base64 usually goes on. A second `close()` writes nothing and succeeds.

1. Waits on this thread as the writer under it does.
2. The same in a task, `co_await armored.async_close()`, over the writer's `async_write`.

A failure of the writer under it, in this close or in a write before, is reported, and the encoder is closed.

## Parameters

None.

## Return value

Nothing, or the `io::error` of the writer under it.

## Complexity

Constant.

## Exceptions

- (1) What the write of the writer under it throws; the writers of the library throw nothing.
- (2) None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // a writer that takes one write and fails every one after
    int writes = 0;
    auto sink = [&](const slice<const byte>& data) -> expected<size_t, io::error> {
        if (++writes > 1) {
            return unexpected<io::error>(io::error(io::errc::closed, "write", "sink"));
        }
        return data.size();
    };
    encoding::base64::encoder armored = encoding::base64::standard.encoder_to(sink);
    println("{}", armored.write("abc").has_value());
    println("{}", armored.write("def").error().message());
    println("{}", armored.close().error().message());
    println("{}", armored.is_closed());
}
```

Output:

```text
true
write sink: stream closed
write sink: stream closed
true
```

## See also

- [write, async_write](write.md): bytes into the encoder
- [is_closed](is_closed.md): whether the encoder was closed
- [sgcl::encoding::base64::encoder](../base64-encoder.md)
