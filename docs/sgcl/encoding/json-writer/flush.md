[sgcl](../../README.md) › [encoding](../README.md) › [json](../json/README.md) › [writer](README.md)

# sgcl::encoding::json::writer::flush, async_flush

```cpp
expected<void, io::error> flush();                                // (1)
async::task<expected<void, io::error>> async_flush() noexcept;    // (2)
```

Hands the text gathered since the last flush to the stream, in one write, and lets it go. An array or an object
still open is not a mistake: what there is is written, and the rest comes with a later flush. A mistake in the
structure made before is reported here, and nothing is written: neither what came after it nor what was gathered
before it. A failure of the stream is kept for good, and every later flush reports it without writing; what was gathered is let
go, and what is written after it is dropped, not gathered, so a long-running writer whose stream failed holds no
more memory.

1. Writes on the thread that calls it.
2. The same in a task: `co_await w.async_flush()`. The text is copied into a managed block the write holds, so a
   stream that writes on the blocking pool never touches memory the task let go of.

## Parameters

None.

## Return value

Nothing, or the first error: the mistake in the structure, an `io::error` whose code is in the encoding category
([errc](../errc.md)`::syntax`, `unsupported_value`) and whose message says what it was; or the error of the
stream.

## Complexity

Linear in the length of the text gathered since the last flush.

## Exceptions

- (1) What the write of the stream throws.
- (2) None from the call, which makes the task; what the write of the stream throws inside it comes out of its
  `co_await`.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer log;
    encoding::json::writer out(log);
    for (int i : range(3)) {
        out.begin_object().key("n").value(i).end_object();
        if (i % 2 == 1) {
            out.flush();
            print("{}", log.text());
        }
    }
    println("{}", log.text().size());
    out.flush();
    println("{}", log.text().size());

    encoding::json::writer wrong(log);
    wrong.value(1).end_array().value(2);
    println(wrong.flush().error().message());
    println("{}", log.text().size());
}
```

Output:

```text
{"n":0}
{"n":1}
16
24
json: end_array without an open array: syntax error
24
```

```cpp
#include "sgcl/async.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<void> report(io::writer out) {
    encoding::json::writer w(out);
    w.begin_object().key("status").value("ok").end_object();
    auto r = co_await w.async_flush();
    println("{}", r.has_value());
}

int main() {
    report(io::stdout).wait();
}
```

Output:

```text
{"status":"ok"}
true
```

## See also

- [(constructor)](json-writer.md): the stream the text goes to
- [io streams](../../io/README.md)
- [sgcl::encoding::json::writer](README.md)
