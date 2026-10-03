[sgcl](../README.md) › [io](README.md)

# sgcl::io::env

```cpp
#include "sgcl/io/os.h"   // or "sgcl/io.h"

namespace sgcl::io {
    /*(1)*/ template<class T>
                requires(!std::is_convertible_v<const T&, string>)
            T env(const string& name, const T& fallback);
    /*(2)*/ template<class Rep, class Period>
            duration env(const string& name, std::chrono::duration<Rep, Period> fallback);
    /*(3)*/ string env(const string& name, const string& fallback) noexcept;
}
```

Returns the environment variable `name` as a value of the fallback's type, or the fallback when the variable is not
set or is empty: `int port = io::env("PORT", 8080)`, `duration t = io::env("TIMEOUT", 5s)`. A program's
configuration in one line, the default written where the value is used.

1. `bool` and the numbers are read as [parse](../core/parse.md) reads them (`"8080"`, `"true"`, `"0.5"`), the
   type's range checked; any other `T` by its `T::parse(const string&)`, which returns an `expected` of a value
   `T` is made from.
2. A span of `<chrono>` as the fallback (`5s`, `250ms`): the value is a [duration](../core/duration.md), read as
   `duration::parse` reads Go's text (`"1.5s"`, `"1m30s"`).
3. A text fallback (a `string`, a literal): the variable's text as it is.

A value that is set and is not one of the type is the program's configuration gone wrong, not data to recover
from: (1) and (2) throw `std::invalid_argument`, whose message names the variable, the value and the type.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name of the variable |
| `fallback` | the value when the variable is not set or is empty; its type is the type of the result |

## Return value

The value of the variable read as the fallback's type, or `fallback`.

## Complexity

Linear in the size of the environment, as the C library's `getenv`, plus the reading of the value.

## Exceptions

- (1–2) `std::invalid_argument` when the variable is set and its text is not a value of the type, with the message
  `sgcl::io::env: NAME="text" is not <the type>: <the reason>`, the type named `a bool`, `an integer`,
  `an unsigned integer`, `a number`, `a duration` or `a value of its type`; what `T::parse` and the copy of `T`
  throw.
- (3) None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    int port = io::env("APP_PORT", 8080);
    duration timeout = io::env("APP_TIMEOUT", 5s);
    string host = io::env("APP_HOST", "localhost");
    println("{}:{}, timeout {}", host, port, timeout);

    (void)io::setenv("APP_PORT", "9090");
    (void)io::setenv("APP_TIMEOUT", "1m30s");
    println("{}, timeout {}", io::env("APP_PORT", 8080), io::env("APP_TIMEOUT", 5s));

    (void)io::setenv("APP_PORT", "abc");
    try {
        port = io::env("APP_PORT", 8080);
    } catch (const std::invalid_argument& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
localhost:8080, timeout 5s
9090, timeout 1m30s
sgcl::io::env: APP_PORT="abc" is not an integer: not a number
```

## See also

- [getenv](getenv.md): the text of a variable, `nullopt` when it is not set
- [flags](flags.md): the command line read into variables
- [parse](../core/parse.md), [duration](../core/duration.md): how the values are read
