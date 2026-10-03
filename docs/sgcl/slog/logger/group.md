[sgcl](../../README.md) › [slog](../README.md) › [logger](../logger.md)

# sgcl::slog::logger::group

```cpp
logger group(const char* name) const noexcept;
```

Returns a logger that puts every later attribute, of its [with](with.md) and of its records, in a group named
`name`, slog's `WithGroup`. In text the group's name stands before the keys with a dot (`req.path=/users`); in JSON
the group is an object (`"req":{"path":"/users"}`). A group that a record would leave empty is not written. A
handler of the program gets the group as an attribute of kind group holding what came after it
([record](../record.md)).

The new logger shares this one's output; this one writes as it did.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name of the group; an empty or a null one is no group, and the logger is a copy |

## Return value

The child logger.

## Complexity

Linear in the size of this logger's attributes, which are copied and rendered again.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    slog::logger text(io::stdout);
    slog::logger json(slog::options{.out = io::stdout, .json = true});
    for (const auto& log : {text, json}) {
        auto http = log.with("service", "api").group("http");
        http.info("request", "path", "/users");
        http.info("nothing in the group");
    }
}
```

Output:

```text
time=2026-09-28T14:05:01.123+02:00 level=INFO msg=request service=api http.path=/users
time=2026-09-28T14:05:01.123+02:00 level=INFO msg="nothing in the group" service=api
{"time":"2026-09-28T14:05:01.123456+02:00","level":"INFO","msg":"request","service":"api","http":{"path":"/users"}}
{"time":"2026-09-28T14:05:01.123456+02:00","level":"INFO","msg":"nothing in the group","service":"api"}
```

## See also

- [with](with.md)
- [sgcl::slog::group](../group.md): a group as one argument of a record
- [sgcl::slog::logger](../logger.md)
