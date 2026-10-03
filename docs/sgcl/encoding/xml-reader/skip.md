[sgcl](../../README.md) › [encoding](../README.md) › [xml](../xml/README.md) › [reader](README.md)

# sgcl::encoding::xml::reader::skip, async_skip

```cpp
bool skip();                                // (1)
async::task<bool> async_skip() noexcept;    // (2)
```

Passes over the node [read](read.md) would give, without building it: an element with everything inside it, a
text, a comment or an instruction the options keep. What `read` leaves out, `skip` passes over too. Go's
`Decoder.Skip()` passes over the rest of the element the decoder is in; this passes over the next node.

1. Reads the stream on the thread that calls it.
2. The same in a task: `co_await r.async_skip()` gives the worker back while the stream waits.

At the end tag of the element the reader is inside, `skip` gives `false` and leaves the end tag for
[next](next.md); `false` at the end of the document and on an error as well.

## Parameters

None.

## Return value

`true` when a node was passed over, `false` where `read` gives `nullopt`.

## Complexity

Linear in the length of the node.

## Exceptions

- (1) What [next](next.md) throws.
- (2) None: what (1) throws, the task's `co_await` or `wait()` throws again.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::xml::reader r("<doc><head><meta/><meta/></head><body>text</body></doc>");
    r.next();
    while (auto t = r.peek()) {
        if (t->is_start("head")) {
            r.skip();
        } else if (t->is_start("body")) {
            println(r.read()->text());
        } else {
            r.next();
        }
    }
}
```

Output:

```text
text
```

## See also

- [read](read.md): the next node whole
- [sgcl::encoding::xml::reader](README.md)
