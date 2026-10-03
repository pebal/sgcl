[sgcl](../../README.md) › [net](../README.md) › [url](../url.md)

# sgcl::net::url::username

```cpp
string username() const noexcept;
```

The user name, escaped as the URL writes it (the standard's userinfo set): `u%20x` of `https://u%20x:p@a.com/`. Go
keeps it in `URL.User`.

## Parameters

None.

## Return value

The user name, or the empty string when there is none.

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
    net::url u("https://jo%20e:s3cret@example.com/");
    println("[{}] [{}]", u.username(), net::url("https://example.com/").username());
}
```

Output:

```text
[jo%20e] []
```

## See also

- [password](password.md): the password
- [with_username](with_username.md): another user name
- [sgcl::net::url](../url.md)
