[sgcl](../../README.md) › [slog](../README.md)

# sgcl::slog::message

```cpp
#include "sgcl/slog/record.h"   // or "sgcl/slog.h"

namespace sgcl::slog {
    class message;
}
```

`sgcl::slog::message` is the first parameter of every verb, [logger::log](../logger/log.md) and the free
[debug, info, warn and error](../debug.md): the text of a record and the place of the call. The text is a literal, a
`const char*`, a [string](../../core/string/README.md), a `std::string` or a text slice, converted where the call is written;
the place is the `std::source_location` that the constructor's default argument takes there. A verb cannot take
the place itself: after the pack of attributes it has no room for a default argument of its own.

## Rules

- It refers to the text, as the call does: it is made in the call that logs and never kept. A record that is kept
  is a [clone](../record/clone.md), which copies the text.
- A null `const char*` is the empty text.
- The place is written with [options](../options.md)`::source`, and read by a handler through
  [record::source](../record/source.md).

## Member functions

| Function | Description |
|---|---|
| [(constructor)](message.md) | constructs the message from a text and the place of the call |

#### Observers

| Function | Description |
|---|---|
| [text](text.md) | the text |
| [where](where.md) | the place of the call |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

void report(const slog::logger& log, slog::message m) {
    log.warn(m, "from", m.where().function_name());
}

int main() {
    slog::logger log(io::stdout);
    log.info("a literal");
    log.info(string("a string"));
    report(log, "the place of this call");
}
```

Output:

```text
time=2026-09-28T14:05:01.123+02:00 level=INFO msg="a literal"
time=2026-09-28T14:05:01.123+02:00 level=INFO msg="a string"
time=2026-09-28T14:05:01.123+02:00 level=WARN msg="the place of this call" from="int main()"
```

## See also

- [logger::log](../logger/log.md), [debug, info, warn, error](../debug.md): the verbs that take it
- [record](../record/README.md): the message and the place, as a handler reads them
- [sgcl::slog](../README.md)
