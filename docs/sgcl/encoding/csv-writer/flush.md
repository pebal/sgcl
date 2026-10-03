[sgcl](../../README.md) › [encoding](../README.md) › [csv](../csv/README.md) › [writer](README.md)

# sgcl::encoding::csv::writer::flush, async_flush

```cpp
expected<void, io::error> flush();                                // (1)
async::task<expected<void, io::error>> async_flush() noexcept;    // (2)
```

Hands the text gathered since the last flush to the stream, in one write, Go's `Flush` and `Error` together. The
writer's first mistake is reported here and kept: a field of a type with no text in CSV
([write](write.md)), whose `io::error` carries the code `encoding::errc::unsupported_value`, or a failed write of
the stream, as the stream reported it. Once the writer holds one, every flush returns it and writes nothing, and
what is written after it is dropped, not gathered: a long-running writer whose stream failed holds no more memory.

1. Writes on the thread that calls it.
2. The same in a task: the write is `co_await`ed, and the worker does other work meanwhile. The text is copied for
   the write, so that a task let go of before the stream finishes leaves the stream nothing freed.

## Parameters

None.

## Return value

Nothing, or the [io::error](../../io/error/README.md).

## Complexity

Linear in the length of the text gathered.

## Exceptions

- (1) What the write of the stream under the writer throws.
- (2) None when the task is made: what the write throws comes out of its `co_await`.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

struct bag {
    vector<int> items;

    void describe(encoding::field_list& f) {
        f.add("items", items);
    }
};

async::task<void> report(io::writer out) {
    encoding::csv::writer w(out);
    for (int i : {1, 2, 3}) {
        w.write({"line", to_string(i)});
        auto flushed = co_await w.async_flush();
        flushed.value();
    }
}

int main() {
    async::run(report(io::stdout));

    encoding::csv::writer w(io::stdout);
    w.write(bag{});
    auto flushed = w.flush();
    println("{}", flushed.error().message());
    println("{}", flushed.error().code() == encoding::errc::unsupported_value);
}
```

Output:

```text
line,1
line,2
line,3
csv: /items: a value CSV has no text for: unsupported value
true
```

## See also

- [write](write.md): a record gathered
- [io::error](../../io/error/README.md): the stream's error
- [sgcl::encoding::csv::writer](README.md)
