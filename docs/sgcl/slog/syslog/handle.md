[sgcl](../../README.md) › [slog](../README.md) › [syslog](README.md)

# sgcl::slog::syslog::handle

```cpp
void handle(const record& r) const;
```

Sends the record as one message: the header of the format, the message, and the module's text of its attributes
(or the structured data); a datagram to the local socket, or one write to the remote writer, octet-counted when the
options say so. What a [logger](../logger/README.md) calls for every record at its level; called by hand for a record
a handler of the program kept.

## Parameters

| Parameter | Description |
|---|---|
| `r` | the record |

## Return value

None.

## Complexity

Linear in the record's text.

## Exceptions

None: a write that fails is counted in [dropped](dropped.md).

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    io::buffer out;
    slog::syslog h(out, {.app_name = "a", .hostname = "h", .structured_data_id = "app@32473"});
    slog::memory kept;
    slog::logger(kept).error("failed", "order", 42);
    h.handle(kept.records()[0]);
    string m = out.text();
    println("{}", m.substr(m.find("[app")));
}
```

Output:

```text
[app@32473 order="42"] failed
```

## See also

- [options](../syslog-options.md)
- [sgcl::slog::syslog](README.md)
