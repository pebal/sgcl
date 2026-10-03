[sgcl](../../README.md) › [net](../README.md) › [url](../url.md)

# sgcl::net::url::with_password

```cpp
expected<url, io::error> with_password(const string& password) const noexcept;
```

The URL with the password given, escaped with the standard's userinfo set; the empty string removes it. The error of a
refused password does not carry it: its message names the setter alone.

## Parameters

| Parameter | Description |
|---|---|
| `password` | the new password, unescaped |

## Return value

The new URL, or an [io::error](../../io/error.md) of the code `net::errc::invalid_url` ([errc](../errc.md)), the
operation `set URL password` and the value asked for, when the standard refuses the value or declines to apply it: a
URL without a host, or a file URL; and a value past 512 MiB, or a URL that would pass it ([the limit](../url.md#rules)).

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
    println(net::url("https://joe@example.com/").with_password("s3:cret")->to_string());
    auto mail = net::url("mailto:joe@example.com").with_password("s3cret");
    println(mail.error().message());
}
```

Output:

```text
https://joe:s3%3Acret@example.com/
set URL password: invalid URL
```

## See also

- [password](password.md): the password
- [with_username](with_username.md): the user name
- [sgcl::net::url](../url.md)
