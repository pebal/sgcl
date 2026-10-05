[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [cookie_jar](README.md)

# sgcl::net::http::cookie_jar::clear_session

```cpp
size_t clear_session() const noexcept;
```

The session cookies out of the jar, those set without `Max-Age` or `Expires`: what a browser drops when it is
restarted. A program that keeps a jar across a long run and starts a new "session" of its own clears them; one that
[saves](save.md) its jar leaves them out of the file by itself.

## Parameters

None.

## Return value

The number of cookies removed.

## Complexity

Linear in the cookies of the jar.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::cookie_jar jar;
    jar.set_cookies(net::url("https://example.com/"),
                    {net::http::cookie("visit=1"), net::http::cookie("login=2; Max-Age=86400")});
    println("{}", jar.clear_session());
    println("{}", jar.header(net::url("https://example.com/")));
}
```

Output:

```text
1
login=2
```

## See also

- [clear_expired](clear_expired.md): the expired cookies out
- [sgcl::net::http::cookie_jar](README.md)
