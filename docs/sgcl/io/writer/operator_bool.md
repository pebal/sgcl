[sgcl](../../README.md) › [io](../README.md) › [writer](../writer.md)

# sgcl::io::writer::operator bool

```cpp
explicit operator bool() const noexcept;
```

Checks whether the writer holds a stream. A default-constructed writer holds none, and so does one made of an empty
handle (a default-constructed [buffered_writer](../buffered_writer.md)) or of a null pointer to a handle or to a
stream of the program's. A field that may have no destination is tested so before it is written to.

## Parameters

None.

## Return value

`true` when the writer holds a stream, `false` when it is empty.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

struct Job {
    io::writer trace;  // none: no tracing
};

int main() {
    Job quiet;
    Job traced{io::stdout};
    for (Job* job : {&quiet, &traced}) {
        if (job->trace) {
            job->trace.write("traced\n");
        }
    }
    io::buffered_writer no_destination;
    println("{}", bool(io::writer(no_destination)));
}
```

Output:

```text
traced
false
```

## See also

- [(constructor)](writer.md)
- [sgcl::io::writer](../writer.md)
