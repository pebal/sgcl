[sgcl](../../README.md) › [encoding](../README.md) › [hex](../hex.md)

# sgcl::encoding::hex::dumper_to

```cpp
static dumper dumper_to(const io::writer& out) noexcept;
```

A writer that writes the dump of what it is given to `out`: Go's `hex.Dumper`. A line goes out as soon as its
sixteen bytes are there, the lines [dump](dump.md) writes, with the offsets counted across the writes;
[close()](../hex-dumper/close.md) writes the short line at the end and leaves `out` open. A failure of `out` is
kept for good: every later `write` and `close` reports it. The [dumper](../hex-dumper.md) is a handle of one word,
made with its state: a managed object holding an 8 KB block and `out`.

## Parameters

| Parameter | Description |
|---|---|
| `out` | the writer the lines go to: any stream of io, a handle of the library, a stream of one's own |

## Return value

The dumper.

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
    // what goes over a wire, dumped as it goes
    encoding::hex::dumper wire = encoding::hex::dumper_to(io::stdout);
    wire.write("a line of twenty bytes");
    wire.close();  // the short line at the end
}
```

Output:

```text
00000000  61 20 6c 69 6e 65 20 6f  66 20 74 77 65 6e 74 79  |a line of twenty|
00000010  20 62 79 74 65 73                                 | bytes|
```

## See also

- [hex::dumper](../hex-dumper.md): the stream
- [dump](dump.md): the dump at once
- [sgcl::encoding::hex](../hex.md)
