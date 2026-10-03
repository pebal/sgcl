[sgcl](../../README.md) › [encoding](../README.md) › [hex](../hex.md) › [encoder](../hex-encoder.md)

# sgcl::encoding::hex::encoder::close, async_close

```cpp
/*(1)*/ expected<void, io::error> close() const;
/*(2)*/ async::task<expected<void, io::error>> async_close() const noexcept;
```

Ends the encoder: a write after it is `io::errc::closed`. A byte is a whole group, so nothing waits and nothing
is written: the digits went out with the writes. The writer under it stays open, as the other encoders of the
module leave theirs. A second `close()` succeeds.

1. Waits on this thread as the writer under it does.
2. The same in a task, `co_await digits.async_close()`.

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
    encoding::hex::encoder digits = encoding::hex::encoder_to(sink);
    println("{}", digits.write("abc").has_value());
    println("{}", digits.write("def").error().message());
    println("{}", digits.close().error().message());
    println("{}", digits.is_closed());
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
- [sgcl::encoding::hex::encoder](../hex-encoder.md)
