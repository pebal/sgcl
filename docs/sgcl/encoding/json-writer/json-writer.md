[sgcl](../../README.md) › [encoding](../README.md) › [json](../json.md) › [writer](../json-writer.md)

# sgcl::encoding::json::writer::writer

```cpp
explicit writer(const io::writer& out) noexcept;           // (1)
writer(const io::writer& out, const style& s) noexcept;    // (2)
writer(const writer&) = delete;                            // (3)
```

Constructs a writer into a stream. Nothing is written until [flush](flush.md). Anything with a `write` is a
stream: a file, a connection, an [io::buffer](../../io/buffer.md), `io::stdout`, an encoder of the module, a
callable.

1. The text compact, with no space at all, as [json::compact](../json.md#member-objects) lays it out.
2. The text laid out by `s`: [json::pretty](../json.md#member-objects) indents by two spaces, as Go's
   `SetIndent("", "  ")`; a [style](../json-style.md) of one's own sets the indent, the escape of `<`, `>` and `&`,
   and whether the keys of a hash map of a typed value are sorted.
3. A writer is neither copied nor moved: two writers would interleave their text in one stream.

## Parameters

| Parameter | Description |
|---|---|
| `out` | the stream the text is handed to |
| `s` | how the text is laid out |

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
    io::buffer compact;
    encoding::json::writer a(compact);
    a.begin_object().key("tag").value("<b>").end_object();
    a.flush();
    print("{}", compact.text());

    encoding::json::style s;
    s.indent = 4;
    s.escape_html = true;
    io::buffer laid_out;
    encoding::json::writer b(laid_out, s);
    b.begin_object().key("tag").value("<b>").end_object();
    b.flush();
    print("{}", laid_out.text());
}
```

Output:

```text
{"tag":"<b>"}
{
    "tag": "\u003cb\u003e"
}
```

## See also

- [json::style](../json-style.md): how the text is laid out
- [flush](flush.md): the text to the stream
- [io streams](../../io/README.md)
- [sgcl::encoding::json::writer](../json-writer.md)
