[sgcl](../../README.md) › [encoding](../README.md) › [xml](../xml/README.md) › [reader](README.md)

# sgcl::encoding::xml::reader::peek, async_peek

```cpp
optional<token> peek();                                // (1)
async::task<optional<token>> async_peek() noexcept;    // (2)
```

The token [next](next.md) gives next, left where it is: a copy of it, the reader keeping its own. The loop that
reads the elements wanted whole and steps over the rest peeks at each token first.

1. Reads the stream on the thread that calls it, when the token is not in the buffer yet.
2. The same in a task: `co_await r.async_peek()` gives the worker back while the stream waits.

`nullopt` at the end of the document and on an error, as `next()` gives it.

## Parameters

None.

## Return value

The next token, or `nullopt` at the end or on an error.

## Complexity

As [next](next.md) the first time; constant after, until the token is taken.

## Exceptions

- (1) What [next](next.md) throws.
- (2) None: what (1) throws, the task's `co_await` or `wait()` throws again.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::xml::reader r("<orders><order id='1'/><note>x</note><order id='2'/></orders>");
    while (auto t = r.peek()) {
        if (t->is_start("order")) {
            println(r.read()->attribute("id", "?"));
        } else {
            r.next();
        }
    }
}
```

Output:

```text
1
2
```

## See also

- [next](next.md): the next token, taken
- [read](read.md), [skip](skip.md): what follows a peek
- [sgcl::encoding::xml::reader](README.md)
