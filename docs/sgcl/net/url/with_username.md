[sgcl](../../README.md) › [net](../README.md) › [url](../url.md)

# sgcl::net::url::with_username

```cpp
expected<url, io::error> with_username(const string& username) const noexcept;
```

The URL with the user name given, escaped with the standard's userinfo set; the empty string removes it.

## Parameters

| Parameter | Description |
|---|---|
| `username` | the new user name, unescaped |

## Return value

The new URL, or an [io::error](../../io/error.md) of the code `net::errc::invalid_url` ([errc](../errc.md)), the
operation `set URL username` and the value asked for, when the standard refuses the value or declines to apply it: a
URL without a host, or a file URL, which can have no credentials; and a value past 512 MiB, or a URL that would pass
it ([the limit](../url.md#rules)).

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
    println(net::url("https://example.com/").with_username("joe doe@corp")->to_string());
    auto file = net::url("file:///x").with_username("joe");
    println(file.error().message());
}
```

Output:

```text
https://joe%20doe%40corp@example.com/
set URL username joe: invalid URL
```

## See also

- [username](username.md): the user name
- [with_password](with_password.md): the password
- [sgcl::net::url](../url.md)
