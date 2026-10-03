[sgcl](../../README.md) › [async](../README.md) › [stop_token](../stop_token.md)

# sgcl::async::stop_token::stop_possible

```cpp
bool stop_possible() const noexcept;
```

Checks whether the token has a source, and so may ever stop. A token made by default has none. It stays `true`
once the stop has been requested, and, unlike `std::stop_token::stop_possible`, when every source is gone without
a stop: the token holds the state, and the state is what it looks at. `on_stop`, `stopped()` and `channel()` need a
source; this is the look before them.

## Parameters

None.

## Return value

`true` when the token came from a [stop_source](../stop_source.md), `false` for a token made by default.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::stop_token none;
    println("{}", none.stop_possible());
    async::stop_source source;
    async::stop_token token = source.token();
    println("{}", token.stop_possible());
    source.request_stop();
    println("{}", token.stop_possible());
}
```

Output:

```text
false
true
true
```

## See also

- [stop_requested](stop_requested.md): whether the stop came
- [(constructor)](stop_token.md): a token with no source
- [sgcl::async::stop_token](../stop_token.md)
