[sgcl](../../README.md) › [io](../README.md) › [process_state](../process_state.md)

# sgcl::io::process_state::operator=

```cpp
process_state& operator=(const process_state& o) noexcept;
```

Copies `o` into this state, field by field.

## Parameters

| Parameter | Description |
|---|---|
| `o` | the state to copy |

## Return value

`*this`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::process_state last;
    io::command cmd("false");
    (void)cmd.run();
    last = *cmd.state;
    println("{}", last.to_string());
}
```

Output:

```text
exit status 1
```

## See also

- [(constructor)](process_state.md): an empty state, or a copy
- [sgcl::io::process_state](../process_state.md)
