[sgcl](../../README.md) › [async](../README.md) › [stop_token](../stop_token.md)

# sgcl::async::stop_token::stop_token

```cpp
stop_token() noexcept = default;
```

A token with no source: it never stops, and [stop_possible](stop_possible.md) is `false`. A token with a source
comes from [stop_source::token](../stop_source/token.md). Copies of a token share its state: a copy is one word,
and the copy and the assignment are the implicit ones, noexcept.

A function that may be given no stop at all takes such a token, as a default argument, and looks at
`stop_possible()` before it waits on it: `on_stop`, `stopped()` and `channel()` of an empty token are an error.

## Parameters

None.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<string> fetch(async::stop_token token = {}) {
    co_return token.stop_possible() ? "may be stopped" : "runs to its end";
}

int main() {
    println("{}", fetch().wait());
    async::stop_source source;
    println("{}", fetch(source.token()).wait());
}
```

Output:

```text
runs to its end
may be stopped
```

## See also

- [stop_source::token](../stop_source/token.md): a token with a source
- [stop_possible](stop_possible.md): whether a token has one
- [sgcl::async::stop_token](../stop_token.md)
