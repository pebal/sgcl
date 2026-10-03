[sgcl](../README.md) › [io](README.md)

# sgcl::io::exit

```cpp
#include "sgcl/io/os.h"   // or "sgcl/io.h"

namespace sgcl::io {
    [[noreturn]] void exit(int code) noexcept;
}
```

Ends the process now with the status `code`, as Go's `os.Exit`: the streams of the C library are flushed, and then
`_exit` ends the process without running a destructor, an `atexit` handler or the destructors of static objects. The
streams of the module have no buffer of their own to lose ([standard_stream](standard_stream.md)); a
[buffered_writer](buffered_writer.md) that was not flushed loses what it holds. The name is qualified in a program,
`io::exit`: under `using namespace sgcl;` a bare `exit(1)` is the C library's.

## Parameters

| Parameter | Description |
|---|---|
| `code` | the exit status: 0 for success, another value for a failure |

## Return value

None: the function does not return.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

struct farewell {
    ~farewell() {
        println("never printed");
    }
};

int main() {
    farewell f;
    println("done");
    io::exit(0);
}
```

Output:

```text
done
```

## See also

- [flags::parse](flags/parse.md): ends the process with 0 or 2 on the command line it reads
- [standard_stream](standard_stream.md): the standard streams, which write at once
