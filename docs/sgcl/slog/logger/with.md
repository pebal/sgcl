[sgcl](../../README.md) › [slog](../README.md) › [logger](README.md)

# sgcl::slog::logger::with

```cpp
template<class... A>
logger with(const A&... kv) const;
```

Returns a logger that writes the attributes `kv` in every record, after those of this logger, slog's `With`. It takes
the pairs and groups a verb takes, checked by the compiler the same way. They are copied and rendered into both
formats once, now; a record adds them as bytes. A type described by its fields, a `to_text` or a `format_value` is
made text here, so the logger may outlive the objects it was given; a container's text form longer than 4 GiB − 1
is cut to fit, at the start of a code point. After a [group](group.md) they go inside that group.

The new logger shares this one's output; this one writes as it did, without the attributes.

## Parameters

| Parameter | Description |
|---|---|
| `kv` | the pairs, a key then a value, and groups |

## Return value

The child logger.

## Complexity

Linear in the size of the attributes, this logger's and the new ones: they are copied and rendered.

## Exceptions

What the text of a value of the program throws (its `to_text`, `to_string` or `format_value`); none for the values
of the library.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    slog::logger base(io::stdout);
    slog::logger api;
    {
        string service = "api";
        api = base.with("service", service, slog::group("build", "version", 3));
    }
    api.info("started", "port", 8080);
    base.info("the base as it was");
}
```

Output:

```text
time=2026-09-28T14:05:01.123+02:00 level=INFO msg=started service=api build.version=3 port=8080
time=2026-09-28T14:05:01.123+02:00 level=INFO msg="the base as it was"
```

## See also

- [group](group.md): the later attributes in a group
- [sgcl::slog::group](../group/README.md): attributes in a group of their own
- [sgcl::slog::logger](README.md)
