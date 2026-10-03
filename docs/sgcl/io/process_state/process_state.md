[sgcl](../../README.md) › [io](../README.md) › [process_state](../process_state.md)

# sgcl::io::process_state::process_state

```cpp
/*(1)*/ process_state() noexcept = default;
/*(2)*/ process_state(const process_state& o) noexcept;
```

1. The state of no process: id 0, exited with 0, no processor time.
2. A copy of `o`, field by field: the class is not trivially copyable on purpose
   ([process_state](../process_state.md)).

A state of a process that ended is made by a wait alone.

## Parameters

| Parameter | Description |
|---|---|
| `o` | the state to copy |

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::process_state none;
    println("{} {}", none.pid(), none.success());
    io::command cmd("true");
    (void)cmd.run();
    io::process_state copy(*cmd.state);
    println("{}", copy.pid() == cmd.process.pid());
    println("{}", std::is_trivially_copyable_v<io::process_state>);
}
```

Output:

```text
0 true
true
false
```

## See also

- [operator=](operator_assign.md): copies a state
- [sgcl::io::process_state](../process_state.md)
