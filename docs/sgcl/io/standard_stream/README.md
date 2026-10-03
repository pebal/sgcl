[sgcl](../../README.md) › [io](../README.md)

# sgcl::io::standard_stream

```cpp
#include "sgcl/io/os.h"   // or "sgcl/io.h"

namespace sgcl::io {
    class standard_stream;

    inline standard_stream stdin(0, "stdin");
    inline standard_stream stdout(1, "stdout");
    inline standard_stream stderr(2, "stderr");
}
```

`standard_stream` is the class of the three standard streams of the process, `io::stdin`, `io::stdout` and
`io::stderr`, Go's `os.Stdin`, `os.Stdout` and `os.Stderr`. Each is an object of its own, constant-initialized,
over a [file](../file/README.md) of the descriptor 0, 1 or 2 that is made the first time the stream is used, by whichever
thread comes first, and kept for the life of the process: it is never destroyed, so a static destructor may still
write to it, and the descriptor is never closed by it. The stream writes at once, through the descriptor, with no
buffer of its own: what [print](../print.md) and [println](../println.md) write is out when they return. The class
carries [mixin::reader](../mixin/reader/README.md) and [mixin::writer](../mixin/writer/README.md).

The descriptor's flags are left as they are. A terminal, a redirected file and a pipe from the shell, which are
blocking, have their asynchronous operations run on the [blocking pool](../../async/spawn_blocking.md); a descriptor the
parent made non-blocking is served by the [reactor](../../async/readable.md).

`<cstdio>` defines `stdin`, `stdout` and `stderr` as macros for its `FILE` streams. `os.h` removes the macros and,
where a macro named another variable (macOS: `__stdinp`, `__stdoutp`, `__stderrp`), binds the C streams under the
same names as references in the global scope, so code written for `<stdio.h>` compiles on (`fprintf(stderr, ...)`);
where the variable carries the name already (glibc), the removal alone does it. A unit with
`using namespace sgcl::io` that writes a bare `stderr` for the C stream finds both and qualifies one.

## Rules

- The three objects are the streams; a `standard_stream` is neither copied nor moved. A stream is passed by
  reference, which an [io::reader](../reader/README.md) or an [io::writer](../writer/README.md) holds as a reference: the objects live
  as long as the process.
- What takes a [file](../file/README.md) is given [file()](file.md): a child's standard stream shared with
  the program's (`cmd.out = io::stdout.file()`).

## Member functions

| Function | Description |
|---|---|
| [(constructor)](standard_stream.md) | a stream over a descriptor |
| [read, async_read](read.md) | reads what is available |
| [write, async_write](write.md) | writes bytes, text or a byte |
| [file](file.md) | the file over the descriptor |
| [fd](fd.md) | the descriptor |
| [is_terminal](is_terminal.md) | checks whether the descriptor is a terminal |

#### From mixin::reader

| Function | Description |
|---|---|
| [read_full, async_read_full](../mixin/reader/read_full.md) | fills a buffer whole |
| [read_all, async_read_all](../mixin/reader/read_all.md) | reads to the end, as bytes |
| [read_all_text, async_read_all_text](../mixin/reader/read_all_text.md) | reads to the end, as text |
| [copy_to, async_copy_to](../mixin/reader/copy_to.md) | copies to the end into a writer |

#### From mixin::writer

| Function | Description |
|---|---|
| [write, async_write](../mixin/writer/write.md) | writes text or a byte |
| [copy_from, async_copy_from](../mixin/writer/copy_from.md) | copies a reader to its end |

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::stdout.write("to the standard output\n");
    (void)io::stderr.write("to the standard error\n");
    auto input = io::stdin.read_all_text();
    println("{} bytes on the standard input", input ? input->size() : 0);
}
```

Output:

```text
to the standard output
to the standard error
0 bytes on the standard input
```

## See also

- [print](../print.md), [println](../println.md), [eprint](../eprint.md), [eprintln](../eprintln.md): formatted text on the
  standard streams
- [is_terminal](../is_terminal.md): whether a descriptor is a terminal
- [file](../file/README.md): the stream over a descriptor
