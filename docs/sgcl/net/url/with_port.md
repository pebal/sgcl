[sgcl](../../README.md) › [net](../README.md) › [url](README.md)

# sgcl::net::url::with_port

```cpp
expected<url, io::error> with_port(optional<uint16_t> port) const noexcept;
```

The URL with the port given, by the standard's port setter; `nullopt` removes the port. The default port of the scheme
is dropped, as the parser drops it: `with_port(80)` of an http URL leaves it without one.

## Parameters

| Parameter | Description |
|---|---|
| `port` | the new port, or `nullopt` for none |

## Return value

The new URL, or an [io::error](../../io/error/README.md) of the code `net::errc::invalid_url` ([errc](../errc.md)), the
operation `set URL port` and the value asked for, when the standard refuses the value or declines to apply it: a URL
without a host, or a file URL; and a URL that would pass 512 MiB ([the limit](README.md#rules)).

## Complexity

Linear in the length of the URL and of the value.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::url u("http://example.com/");
    println(u.with_port(8080)->to_string());
    println(u.with_port(8080)->with_port(nullopt)->to_string());
    println(u.with_port(80)->to_string());
    println(net::url("file:///x").with_port(21).error().message());
}
```

Output:

```text
http://example.com:8080/
http://example.com/
http://example.com/
set URL port 21: invalid URL
```

## See also

- [port](port.md), [effective_port](effective_port.md): the port
- [sgcl::net::url](README.md)
