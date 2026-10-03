[sgcl](../README.md) › [io](README.md)

# sgcl::io::expand_env

```cpp
#include "sgcl/io/os.h"   // or "sgcl/io.h"

namespace sgcl::io {
    string expand_env(const string& text) noexcept;
}
```

Returns `text` with every `$NAME` and `${NAME}` replaced by the value of the variable, a variable that is not set by
nothing: Go's `os.ExpandEnv`. A name after a bare `$` is the longest run of letters, digits and `_`; inside the
braces it is everything up to the `}`. A `$` followed by no name, the last character, or a `${` with no `}` after it
stays as it is.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text with the variables to replace |

## Return value

The text with the variables replaced.

## Complexity

Linear in the length of the text and the values, plus a lookup of each variable.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::setenv("APP_USER", "ada");
    println("{}", io::expand_env("/home/$APP_USER/${APP_USER}.log"));
    println("[{}] [{}]", io::expand_env("$APP_NOT_SET"), io::expand_env("cost: 5$"));
}
```

Output:

```text
/home/ada/ada.log
[] [cost: 5$]
```

## See also

- [getenv](getenv.md): one variable
- [environ](environ.md): every variable
