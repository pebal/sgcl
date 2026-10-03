[sgcl](../../README.md) › [net](../README.md) › [url](../url.md)

# sgcl::net::url::password

```cpp
string password() const noexcept;
```

The password, escaped as the URL writes it. Go keeps it in `URL.User`.

## Parameters

None.

## Return value

The password, or the empty string when there is none.

## Complexity

Linear in the length of the part.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::url u("https://joe:s3cret@example.com/");
    println("{} {}", u.username(), u.password());
}
```

Output:

```text
joe s3cret
```

## See also

- [username](username.md): the user name
- [with_password](with_password.md): another password
- [sgcl::net::url](../url.md)
