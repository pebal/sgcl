[sgcl](../README.md) › io

# sgcl::io

```cpp
#include "sgcl/io.h"   // namespace sgcl::io
```

What Go has in `os`, `io`, `bufio`, `path/filepath` and `os/exec`: files and the file system, streams over them and
over anything else that reads or writes, buffering, paths as strings, files mapped into memory and memory shared
between processes, the process and its environment, the command line, and a child process with its streams. The
module depends on [core](../core/README.md) and [async](../async/README.md), whose blocking pool and reactor carry
its asynchronous side; `net`, `compress`, `codec` and `encoding` are built on its streams.

The idea it rests on is that a stream is whatever has the primitive. A reader is a `read(slice<byte>)` that returns
the bytes read, 0 at the end; a writer a `write(slice<const byte>)` that writes all of it or fails; where a stream can
wait without holding a thread, `async_read` and `async_write` return a task. No base class and nothing virtual: the
requirements ([req](req/README.md)) are concepts checked where a stream is passed, a lambda of the same shape is a stream
too, the functions of the module ([copy](copy.md), [read_full](read_full.md), [read_all](read_all.md),
[write](write.md)) take any of them, the classes of the library have the same as methods from the
[mixins](mixin/README.md), and [reader](reader/README.md) and [writer](writer/README.md) hold any stream as a value, Go's interface
value, where one has to be kept. Every operation that waits has two names: `read()` takes the thread until the data
comes, `co_await async_read()` gives the worker back meanwhile.

Errors are values. An operation that can fail returns [expected\<T, io::error\>](error/README.md): the value, or an `error`
that carries the code (`errno` in the system category, or one of the module's own, [errc](errc.md)), the operation
and the path, so that `e.message()` reads `open log.txt: No such file or directory`, and that answers the questions
a caller asks (`is_not_found()`, `is_exists()`, `is_permission()`, `is_closed()`, `is_eof()`, `is_timeout()`). A
missing file, a reset connection, a full disk are outcomes handled where they occur, which a return value states and
an exception hides; and in a server a `throw` per dropped connection would be the most expensive path of the
program. The end of a stream is not an error: a `read` returns 0. `io::open(p).value()` throws
`bad_expected_access` with the error inside for the code that wants an exception.

## The rules

1. The names of the module are qualified, `io::open`, `io::remove`: `io` has a namespace of its own, as every module
   but `core` has, and its names (`open`, `remove`, `rename`, `stat`, `chdir`, `getenv`, `pipe`, `exit`, `symlink`)
   are the names of the C library, so that in a program with `using namespace sgcl;` a bare `remove("x")` would be
   resolved to libc's `::remove(const char*)`, an exact match over a conversion to `string`, and compile to
   something else. `io::remove("x")` cannot, and reads as Go's `os.Remove`. The four printing functions
   ([print](print.md), [println](println.md), [eprint](eprint.md), [eprintln](eprintln.md)) are also names of
   `sgcl` itself.
2. The objects of the module — [file](file/README.md), [buffer](buffer/README.md), [buffered_reader](buffered_reader/README.md),
   [buffered_writer](buffered_writer/README.md), [process](process/README.md), [mapping](mapping/README.md),
   [shared_memory](shared_memory/README.md) — are handles of one word, a `tracked_ptr` to the object inside, as a
   [string](../core/string/README.md) is: made as values (`io::file f = io::open(p);`, `io::buffer out;`), copied and
   passed by value, the copies sharing one object. A stream made of one ([reader](reader/README.md), [writer](writer/README.md))
   holds that object, not the handle. A handle lives on a stack, in a task, in a managed object; in a global or a
   `std` container it is held by a [rooted](../core/rooted/README.md) (`rooted<io::file> log(io::open(p));`, then
   `log->write(...)`), never in a managed object or a task's frame, since a root is never part of a cycle.
3. The destructor of a stream runs on the collector's thread, after the sweep that finds the object dead, which may
   be long after the last use: a descriptor is held until then. A stream that is done is `close()`d, which releases
   what it holds now and reports the error a deferred close cannot.
4. A function that waits on a thread and its `async_` form for a task do the same: a regular file's asynchronous
   operation runs the call on the [blocking pool](../async/spawn_blocking.md), since a disk has no readiness to wait for;
   a non-blocking descriptor's (a [pipe](pipe.md), a socket) waits for readiness on the
   [reactor](../async/readable.md). An async operation given a slice without an owner that runs on the pool goes
   through a managed block: the pool's thread never touches plain memory that died with the frame of a task let go
   of. A slice with an owner (a `string`, a `vector`, a `buffer`) is used as it is: give one.
5. One thread or task at a time on one stream; two readers of one file use [read_at](file/read_at.md) with positions
   of their own.

### Buffers

The module owns two kinds of buffer. A block the library keeps in front of a stream and hands slices of (a
[buffered_reader](buffered_reader/README.md)'s) is a managed `array<byte, N>` behind a `tracked_ptr`, with `N` a divisor of
the page (`config::io_buffer_size`, 8 KB: eight to a page), one object with no header and no pointer map. A block
nothing hands a slice of, and that no operation on the blocking pool is given, is unmanaged: the block of
[copy](copy.md) (`config::io_copy_buffer_size`, 32 KB) lies on the stack of the call, a
[buffered_writer](buffered_writer/README.md)'s is its own until its first async operation, and [read_all](read_all.md)
gathers in plain memory before it makes its result. A block an async read or write is handed is managed (that of
`async_copy`, the reads of `async_read_all`), since the operation may run on the pool and outlive the frame of a task
let go of.

Data whose size is the data's (what `read_all` returns, a directory listing, a path) is a `vector<byte>` or a
[string](../core/string/README.md); a `string` is one word, so handing one back or taking one costs nothing beyond making
it, and every text parameter of the module is a `const string&` (a literal makes one). A range of bytes handed to
`read` or `write`, or handed back by `peek` and `buffer::data()`, is a [slice](../core/slice/README.md): `slice<byte>`,
`slice<const byte>`, the elements and the managed object they lie in, held — a `std::span` when the memory is
unmanaged — so a stream reading into a vector's buffer keeps the vector alive for as long as the read runs.

### Paths and text

Paths are strings, in the platform's form (`/` on POSIX); [path](path/README.md) is the lexical operations on them, a
`string` in and a `string` out, no path type. Text is UTF-8 in `char`: a `string` is bytes, `size()` counts them,
and [path::match](path/match.md) compares code points (`?` is one character, `[α-ω]` a range of them). Invalid bytes
are rejected nowhere; on POSIX a path is bytes the system does not interpret.

### Mapped memory

[map](map.md) maps a file into memory and [shared_memory](shared_memory/README.md) is a named region shared between
processes; both hand out the bytes as a slice whose owner is the region (one managed object under both handles,
unmapped when nothing holds it any more), so a slice kept after the handle still reads them. The region is outside
the managed heap: only trivial data goes into it, never a `tracked_ptr` or a handle of the library.

## Functions

| Function | Header | Description |
|---|---|---|
| [append_file, async_append_file](append_file.md) | `file.h` | bytes added at the end of a file, the file created when missing |
| [args](args.md) | `os.h` | the command line, `argv[0]` first, with no `main` needed |
| [cache_dir](cache_dir.md) | `os.h` | the directory the platform names for cached data |
| [category](category.md) | `error.h` | the error category of `errc` |
| [chdir](chdir.md) | `os.h` | changes the working directory |
| [chmod, async_chmod](chmod.md) | `fs.h` | sets the permissions of a file |
| [config_dir](config_dir.md) | `os.h` | the directory the platform names for configuration |
| [copy, async_copy](copy.md) | `functions.h` | a reader to its end into a writer |
| [copy_file, async_copy_file](copy_file.md) | `fs.h` | the bytes and the permissions of a file copied |
| [create, async_create](create.md) | `file.h` | a file opened for writing, created or emptied |
| [env](env.md) | `os.h` | a variable as a value of the fallback's type, the fallback when unset |
| [environ](environ.md) | `os.h` | every variable of the environment |
| [eprint](eprint.md) | `print.h` | formatted text on the standard error |
| [eprintln](eprintln.md) | `print.h` | formatted text and a new line on the standard error |
| [executable](executable.md) | `os.h` | the path of the running program |
| [exists](exists.md) | `fs.h` | checks whether something is at a path |
| [exit](exit.md) | `os.h` | ends the process now, no destructor run |
| [expand_env](expand_env.md) | `os.h` | `$NAME` and `${NAME}` in a text replaced by the variables |
| [from_fd](from_fd.md) | `file.h` | a file over a descriptor opened elsewhere |
| [getenv](getenv.md) | `os.h` | a variable, `nullopt` when it is not set |
| [home_dir](home_dir.md) | `os.h` | the home directory of the user |
| [hostname](hostname.md) | `os.h` | the name of the host |
| [is_directory](is_directory.md) | `fs.h` | checks whether a path names a directory |
| [is_regular](is_regular.md) | `fs.h` | checks whether a path names a regular file |
| [is_terminal](is_terminal.md) | `os.h` | checks whether a descriptor is a terminal |
| [last_error](last_error.md) | `error.h` | the error of `errno` with an operation and a path |
| [look_path](look_path.md) | `exec.h` | the executable a name stands for, as `PATH` finds it |
| [lstat, async_lstat](lstat.md) | `fs.h` | what the file system says of a path, a symbolic link not followed |
| [make_error_code](make_error_code.md) | `error.h` | an `errc` as a `std::error_code` |
| [make_temp_dir, async_make_temp_dir](make_temp_dir.md) | `file.h` | a new directory of a random name |
| [map](map.md) | `mapping.h` | a file mapped into memory |
| [mkdir, async_mkdir](mkdir.md) | `fs.h` | makes one directory |
| [mkdir_all, async_mkdir_all](mkdir_all.md) | `fs.h` | makes a directory and its missing parents |
| [open, async_open](open.md) | `file.h` | opens a file with flags and permissions |
| [pid](pid.md) | `os.h` | the id of the process |
| [pipe](pipe.md) | `file.h` | an anonymous pipe, both ends on the reactor |
| [print](print.md) | `print.h` | formatted text on the standard output or a writer |
| [println](println.md) | `print.h` | formatted text and a new line on the standard output or a writer |
| [read_all, async_read_all](read_all.md) | `functions.h` | a reader to its end, as bytes |
| [read_all_text, async_read_all_text](read_all_text.md) | `functions.h` | a reader to its end, as text |
| [read_dir, async_read_dir](read_dir.md) | `fs.h` | the entries of a directory, sorted by name |
| [read_file, async_read_file](read_file.md) | `file.h` | a whole file as bytes |
| [read_full, async_read_full](read_full.md) | `functions.h` | a buffer filled whole from a reader |
| [read_lines, async_read_lines](read_lines.md) | `file.h` | the lines of a text file, without their ends |
| [read_link](read_link.md) | `fs.h` | the target of a symbolic link |
| [read_text, async_read_text](read_text.md) | `file.h` | a whole file as a string |
| [remove, async_remove](remove.md) | `fs.h` | removes a file, a link or an empty directory |
| [remove_all, async_remove_all](remove_all.md) | `fs.h` | removes a path and everything under it |
| [rename, async_rename](rename.md) | `fs.h` | moves a file, replacing what is at the new path |
| [set_modified](set_modified.md) | `fs.h` | sets the time of the last modification |
| [setenv](setenv.md) | `os.h` | sets a variable |
| [stat, async_stat](stat.md) | `fs.h` | what the file system says of a path |
| [symlink, async_symlink](symlink.md) | `fs.h` | makes a symbolic link |
| [temp_dir](temp_dir.md) | `os.h` | the directory of temporary files |
| [temp_file, async_temp_file](temp_file.md) | `file.h` | a new file of a random name |
| [unsetenv](unsetenv.md) | `os.h` | removes a variable |
| [walk_dir, async_walk_dir](walk_dir.md) | `fs.h` | every entry under a directory, in lexical order |
| [working_dir](working_dir.md) | `os.h` | the working directory |
| [write, async_write](write.md) | `functions.h` | bytes, text or a byte written to a writer |
| [write_file, async_write_file](write_file.md) | `file.h` | a whole file written in one call |

## Classes

| Class | Header | Description |
|---|---|---|
| [buffer](buffer/README.md) | `stream.h` | bytes in memory, read from the front and written at the back: Go's `bytes.Buffer` |
| [buffered_reader](buffered_reader/README.md) | `buffered.h` | lines, tokens and prefixes of any reader as slices of a block: Go's `bufio.Reader` and `Scanner` |
| [buffered_writer](buffered_writer/README.md) | `buffered.h` | a block in front of any writer, written out by `flush`: Go's `bufio.Writer` |
| [command](command/README.md) | `exec.h` | a program to run, its arguments and streams: Go's `exec.Cmd` |
| [directory_entry](directory_entry/README.md) | `fs.h` | an entry of a directory listing: the name, the path, the type |
| [discard_writer](discard_writer/README.md) | `stream.h` | the writer that drops everything, the class of `io::discard` |
| [error](error/README.md) | `error.h` | the error of an operation: a code, the operation, the path |
| [file](file/README.md) | `file.h` | one class for every descriptor: a stream with a position, a handle of one word |
| [file_info](file_info/README.md) | `fs.h` | what a stat says: the name, the size, the type, the permissions, the time |
| [flags](flags/README.md) | `flags.h` | the command line as Go's `flag` package reads it |
| [limit_reader](limit_reader/README.md) | `stream.h` | the first `n` bytes of a reader |
| [map_options](map_options.md) | `mapping.h` | how a file is mapped: writable, shared, a range |
| [mapping](mapping/README.md) | `mapping.h` | a file mapped into memory, its bytes a slice that keeps the mapping |
| [multi_reader](multi_reader/README.md) | `stream.h` | readers one after another |
| [multi_writer](multi_writer/README.md) | `stream.h` | every write to each of several writers |
| [pipe_ends](pipe_ends.md) | `file.h` | the two ends of a pipe |
| [process](process/README.md) | `exec.h` | a running child: its id, a signal, the wait |
| [process_state](process_state/README.md) | `exec.h` | how a process ended: the exit code, the signal, the times |
| [reader](reader/README.md) | `stream.h` | any reader held as a value: Go's `io.Reader` |
| [shared_memory](shared_memory/README.md) | `shared_memory.h` | a named region of memory shared between processes |
| [standard_stream](standard_stream/README.md) | `os.h` | the class of `io::stdin`, `io::stdout`, `io::stderr` |
| [tee_reader](tee_reader/README.md) | `stream.h` | a reader whose bytes are written to a writer as well |
| [transform_reader\<F\>](transform_reader/README.md) | `stream.h` | a reader whose bytes a function changes as they are read |
| [writer](writer/README.md) | `stream.h` | any writer held as a value: Go's `io.Writer` |

## Enumerations

| Enumeration | Header | Description |
|---|---|---|
| [errc](errc.md) | `error.h` | the module's own error codes |
| [file_type](file_type.md) | `fs.h` | the type of a file: regular, directory, symbolic link... |
| [open_flags](open_flags.md) | `file.h` | how a file is opened: read, write, create, truncate, append... |
| [permissions](permissions.md) | `fs.h` | the mode bits of a file |
| [seek_from](seek_from.md) | `req.h` | where a seek counts from |
| [walk_action](walk_action.md) | `fs.h` | what the function of a walk returns: next, skip the directory, stop |

## Objects and types

| Name | Header | Description |
|---|---|---|
| `discard` | `stream.h` | `inline discard_writer discard;`: the writer that drops everything, Go's `io.Discard` ([discard_writer](discard_writer/README.md)) |
| `file_time` | `fs.h` | `std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds>`: the time of a file ([file_info](file_info/README.md)) |
| `stderr` | `os.h` | `inline standard_stream stderr;`: the standard error, descriptor 2 ([standard_stream](standard_stream/README.md)) |
| `stdin` | `os.h` | `inline standard_stream stdin;`: the standard input, descriptor 0 |
| `stdout` | `os.h` | `inline standard_stream stdout;`: the standard output, descriptor 1 |

## Namespaces

| Namespace | Header | Description |
|---|---|---|
| [path](path/README.md) | `path.h` | the lexical operations on paths: `clean`, `join`, `base`, `dir`, `ext`, `rel`, `match`, `glob`... |

## Mixins

The bases a stream of the library declares itself by, and the members each gives it
([the mixins](mixin/README.md), `namespace sgcl::io::mixin`).

| Mixin | Description |
|---|---|
| [reader\<Derived\>](mixin/reader/README.md) | `read_full`, `read_all`, `read_all_text`, `copy_to` over the class's `read` |
| [seeker\<Derived\>](mixin/seeker/README.md) | `tell`, `size`, `rewind` over the class's `seek` |
| [writer\<Derived\>](mixin/writer/README.md) | the `write` of text and of a byte, `copy_from`, over the class's `write` |

## Requirements

What a function of the module asks of a stream ([req](req/README.md), `namespace sgcl::io::req`).

| Requirement | Description |
|---|---|
| [closer, async_closer](req/closer.md) | a stream with `close()`, or `async_close()` for a task |
| [reader, async_reader](req/reader.md) | a stream read with `read(slice<byte>)`, or `async_read` for a task, or a callable of that shape |
| [seeker](req/seeker.md) | a stream with `seek(int64_t, seek_from)` |
| [writer, async_writer](req/writer.md) | a stream written with `write(slice<const byte>)`, or `async_write` for a task, or a callable of that shape |

## See also

- [Benchmarks](benchmarks.md): the streams, the files and the child processes against Go and `std`
- [async](../async/README.md): the blocking pool and the reactor under the async forms
- [net](../net/README.md): connections, streams of the module
- [The modules](../README.md)
