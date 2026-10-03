[sgcl](../../README.md) › [net](../README.md) › [url](README.md)

# sgcl::net::url::with_scheme

```cpp
expected<url, io::error> with_scheme(const string& scheme) const noexcept;
```

The URL with the scheme given, by the standard's protocol setter: the scheme is lowercased, and the default port of
the new scheme is dropped. A special scheme is not changed into one that is not, nor the other way.

## Parameters

| Parameter | Description |
|---|---|
| `scheme` | the new scheme, without the `:` |

## Return value

The new URL, or an [io::error](../../io/error/README.md) of the code `net::errc::invalid_url` ([errc](../errc.md)), the
operation `set URL scheme` and the value asked for, when the standard refuses the value or declines to apply it: a
text that is not a scheme; a special scheme for one that is not, or the other way; and a value past 512 MiB, or a URL
that would pass it ([the limit](README.md#rules)).

## Complexity

Linear in the length of the URL and of the value.

## Exceptions

None.

## Notes

Go has no setters: a field of `url.URL` is set to anything, a URL or not. Here a setter of the standard makes a new
`url`, so every `url` is a URL.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::url u("http://example.com:443/");
    println(u.with_scheme("HTTPS")->to_string());
    for (const char* scheme : {"mailto", "1http"}) {
        auto changed = u.with_scheme(scheme);
        println(changed ? changed->to_string() : changed.error().message());
    }
}
```

Output:

```text
https://example.com/
set URL scheme mailto: invalid URL
set URL scheme 1http: invalid URL
```

## See also

- [scheme](scheme.md): the scheme
- [sgcl::net::url](README.md)
