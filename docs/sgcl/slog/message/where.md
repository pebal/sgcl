[sgcl](../../README.md) › [slog](../README.md) › [message](README.md)

# sgcl::slog::message::where

```cpp
const std::source_location& where() const noexcept;
```

Returns the place the message stands for: the call that made it, unless the constructor was given another. The file
is named as it was given to the compiler, the function as the compiler names it.

## Parameters

None.

## Return value

The `std::source_location` of the call.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

void audit(slog::message m) {
    println("{} at line {} of {}", m.text(), m.where().line(), m.where().function_name());
}

int main() {
    audit("called");
}
```

Output:

```text
called at line 11 of int main()
```

## See also

- [text](text.md)
- [record::source](../record/source.md): the place as a handler reads it
- [sgcl::slog::message](README.md)
