[sgcl](../../README.md) › [encoding](../README.md) › [quoted_printable](../quoted_printable/README.md) › [encoder](README.md)

# sgcl::encoding::quoted_printable::encoder::close, async_close

```cpp
expected<void, io::error> close() const;                                // (1)
async::task<expected<void, io::error>> async_close() const noexcept;    // (2)
```

Writes what waits — a space or a tab at the end of the text, escaped, a CR alone — and closes the encoder; the
writer under it stays open. A second close does nothing and succeeds.

1. Waits on this thread as the writer under it does.
2. The same in a task.

## Parameters

None.

## Return value

Nothing, or the `io::error` of the writer under it (one kept from an earlier write included).

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
    io::buffer out;
    auto enc = encoding::quoted_printable::standard.encoder_to(out);
    enc.write("tab\t");
    enc.close();
    println("{} {}", out.text(), enc.is_closed());
    println("{}", bool(enc.close()));
}
```

Output:

```text
tab=09 true
true
```

## See also

- [write, async_write](write.md)
- [is_closed](is_closed.md)
- [encoder](README.md)
