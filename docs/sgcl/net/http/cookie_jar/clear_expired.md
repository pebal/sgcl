[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [cookie_jar](README.md)

# sgcl::net::http::cookie_jar::clear_expired

```cpp
size_t clear_expired() const noexcept;
```

The cookies whose `Max-Age` or `Expires` has passed out of the jar, which drops them anyway as it finds them (a
request to their site, a store past a limit): for a [size](size.md) that counts the live ones alone, or memory given
back after a long run.

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
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    async::manual_clock clock;  // the library's time, moved by the program
    clock.install();
    net::http::cookie_jar jar;
    jar.set_cookies(net::url("https://example.com/"),
                    {net::http::cookie("short=1; Max-Age=5"), net::http::cookie("long=2; Max-Age=60")});
    clock.advance(std::chrono::seconds(10));
    println("{}", jar.size());
    println("{} {}", jar.clear_expired(), jar.size());
}
```

Output:

```text
2
1 1
```

## See also

- [clear_session](clear_session.md): the session cookies out
- [sgcl::net::http::cookie_jar](README.md)
