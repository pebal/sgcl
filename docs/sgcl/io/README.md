# sgcl::io

What Go has in `os`, `io`, `bufio`, `path/filepath` and `os/exec`: files and the file system, streams over them and over anything else that reads or writes, buffering, paths as strings, the process and its environment, and a child process with its streams. `#include "sgcl/io/io.h"` brings the module in; it depends on [`core`](../core/README.md), [`containers`](../containers/README.md) and [`async`](../async/README.md) (the blocking pool and the reactor carry its asynchronous side), and `net`, `compress` and `codec` are built on its streams.; the index of the whole interface is [`docs/sgcl/`](../README.md).

## The namespace

`io` is the first module in a namespace of its own, `sgcl::io`, and every module after it (`text`, `net`, …) has one too; the four before it are the core of the library and stay flat in `sgcl::`, as `std::vector` and `std::thread` are flat while `std::filesystem::remove` is not. The reason is the same as the standard library's: the names of this module — `open`, `remove`, `rename`, `stat`, `chdir`, `getenv`, `pipe`, `exit` — are the names of libc, and in a program with `using namespace sgcl;` a bare `remove("x")` would be resolved to libc's `::remove(const char*)`, an exact match over a conversion to `string`, and compile to something else. `io::remove("x")` cannot. The names are short, so the qualification reads as Go's `os.Remove`.

## Errors are values

Nothing in the module throws. Every operation returns `result<T>`, an [`expected<T, error>`](error.md): the value, or an `error` that carries the code (`errno` in the system category, or one of the module's own, `errc`), the operation and the path, so that `e.message()` reads `open log.txt: no such file or directory`, and answers the questions a caller asks (`is_not_found()`, `is_exists()`, `is_permission()`, `is_closed()`, `is_eof()`, `is_timeout()`). A missing file, a reset connection, a full disk are outcomes the code handles where they occur, which a return value states and an exception hides; and in a server a `throw` per dropped connection, a microsecond and a lock on the unwinder each, would be the most expensive path of the program. The end of a stream is not an error: a `read` returns 0. `result<T>` is a superset of the other style: `io::open(p).value()` throws `bad_expected_access` with the error inside for the code that wants that.

## Streams

A stream is an interface of one primitive: [`reader::read(slice<byte>)`](stream.md), `writer::write(slice<const byte>)`, `seeker::seek`, `closer::close`, each pure virtual, and a mixin over the primitive with everything else (`mixin::reader`, `mixin::writer`, `mixin::seeker`, as [`mixin::enumerable`](../core/mixin/enumerable.md) is a mixin over `begin()` and `end()`): `read_full`, `read_all`, `read_all_text`, `copy_to`, `write_text`, `copy_from`, `tell`, `size`, `rewind`. A stream is a managed object held by a `tracked_ptr`; a `tracked_ptr<reader>` holds any of them, so a [`buffered_reader`](buffered.md), a gzip reader or a TLS stream takes whatever reads and no template parameter carries the type upward — what an `io.Reader` value is in Go, with the method table in the object rather than beside the pointer. Every operation exists twice: `read()` takes the thread until the data comes, `co_await async_read()` gives the worker back meanwhile; the mixin's operations have both forms too.

The destructor of a stream runs on the collector's thread, after the sweep that finds the object dead, which may be long after the last use: a file is closed then, its descriptor held until then. A stream that is done is `close()`d, which releases what it holds now and reports the error a deferred close cannot.

## Files

[`file`](file.md) is one class for every descriptor: a regular file, a pipe, a terminal, later a socket. `open`, `create`, `from_fd`, `pipe` make one; `read_at`/`write_at` are `pread`/`pwrite`, `stat`, `sync`, `truncate`, `chmod` the calls of the same names. The asynchronous form of an operation goes one of two ways, chosen when the file is made: a regular file's (and a terminal's) runs the call on the [blocking pool](../async/blocking.md), since a disk has no readiness to wait for; a non-blocking descriptor's (a pipe from `pipe()`, a socket) waits for readiness on the [reactor](../async/reactor.md) and makes the call when it will not block. `read_file`, `read_text`, `write_file`, `append_file` do the whole thing in a call, `temp_file` and `temp_dir` make something to clean up.

## Buffers

The module owns two kinds of buffer. A block the library keeps in front of a stream ([`buffered_reader`](buffered.md)'s, `copy()`'s) is a managed `array<std::byte, N>` behind a `tracked_ptr`, with `N` a divisor of the page (`config::IoBufferSize` 8 KB: eight to a page; `config::IoCopyBufferSize` 32 KB: two), one object with no header and no pointer map, from the thread's pool. Data whose size is the data's — what `read_all` returns, a directory listing, a path — is a `vector<std::byte>` or a [`string`](../core/string.md); a `string` is one word, so handing one back or taking one costs nothing beyond making it, and every text parameter of the module is a `const string&` (a literal makes one). A range of bytes handed to `read` or `write`, or handed back by `peek` and `buffer::data()`, is a [`slice`](../core/slice.md): `slice<std::byte>`, `slice<const std::byte>`, the elements and the managed object they lie in, held — a `std::span` when the memory is unmanaged, and `v.as_slice()`, `s.as_slice()` or the block itself when it is managed, so a stream reading into a vector's buffer keeps the vector alive for as long as the read runs. A `buffered_reader` hands out lines as `slice<const char>` of its block, nothing allocated per line and the text interface on the spot; a line kept past the next read is copied first (`sgcl::string(line)`); a line longer than the block is assembled in a vector the reader owns, and `set_max_line` bounds it where the stream is not trusted.

## Paths and the file system

Paths are strings, in the platform's form; [`path`](path.md) is the lexical operations on them (`clean`, `join`, `base`, `dir`, `ext`, `rel`, `match`, `glob`), a `string` in and a `string` out, no path type. [`fs`](fs.md) is what is at a path and making, moving and removing things there (`stat`, `mkdir_all`, `remove_all`, `rename`, `read_dir`, `walk_dir`), `std::filesystem` and the platform under a layer that returns `result<T>`. [`os`](os.md) is the process: `args`, `getenv`, `working_dir`, `home_dir`, `executable`, the standard streams as files (`stdin()`, `stdout()`, `stderr()`: the macros of `<cstdio>` are removed and the C streams re-bound under the same names, so both compile).

Text is UTF-8 in `char`: a `string` is bytes, `size()` counts them, and `path::match` compares code points (`?` is one character, `[α-ω]` a range of them). Invalid bytes are not rejected anywhere; on POSIX a path is bytes the system does not interpret.

## Pages

| page | header | what it is |
|---|---|---|
| [error, result](error.md) | `sgcl/io/error.h` | `errc` (the module's own codes), `error` (code, operation, path; the predicates), `result<T>` |
| [stream](stream.md) | `sgcl/io/stream.h` | `reader`, `writer`, `seeker`, `closer`, `stream`; the mixins `mixin::reader`, `mixin::writer`, `mixin::seeker`; `copy`, `limit_reader`, `tee_reader`, `multi_reader`, `multi_writer`, `discard`, `buffer` |
| [buffered](buffered.md) | `sgcl/io/buffered.h` | `buffered_reader` (`read_line`, `read_until`, `peek`, `lines()`), `buffered_writer` (`flush`) |
| [file](file.md) | `sgcl/io/file.h` | `file`, `open_flags`, `open`, `create`, `from_fd`, `pipe`, `read_file`, `read_text`, `write_file`, `append_file`, `temp_file`, `temp_dir` |
| [fs](fs.md) | `sgcl/io/fs.h` | `permissions`, `file_type`, `file_info`, `dir_entry`, `stat`, `lstat`, `exists`, `mkdir`, `mkdir_all`, `remove`, `remove_all`, `rename`, `copy_file`, `symlink`, `read_link`, `chmod`, `set_modified`, `read_dir`, `walk_dir` |
| [path](path.md) | `sgcl/io/path.h` | `clean`, `join`, `base`, `dir`, `ext`, `stem`, `split`, `split_list`, `is_abs`, `abs`, `rel`, `match`, `glob`, `from_slash`, `to_slash` |
| [exec](exec.md) | `sgcl/io/exec.h` | `command` (the fields of `exec.Cmd`: `path`, `args`, `dir`, `env`, `in`, `out`, `err`, `stop`; `start`, `wait`, `run`, `output`, `combined_output`, `stdin_pipe`…, the `async_` forms), `process` (`pid`, `signal`, `kill`, `wait`, `release`), `process_state`, `look_path` |
| [os](os.md) | `sgcl/io/os.h` | `args`, `getenv`, `setenv`, `unsetenv`, `environ`, `expand_env`, `working_dir`, `chdir`, `home_dir`, `cache_dir`, `config_dir`, `temp_path`, `executable`, `hostname`, `pid`, `stdin`, `stdout`, `stderr`, `is_terminal`, `exit` |

## SGCL and Go

| Go | sgcl::io | note |
|---|---|---|
| `io.Reader`, `io.Writer`, `io.Seeker`, `io.Closer` | `reader`, `writer`, `seeker`, `closer` | one pure virtual method each; `stream` is `ReadWriteCloser` |
| `io.ReadAll`, `io.ReadFull`, `io.Copy`, `io.CopyN` | `r->read_all()`, `r->read_full(b)`, `copy(w, r)`, `copy(w, *sgcl::make_tracked<limit_reader>(r, n))` | members of every reader through the mixin; a `co_await async_` form of each |
| `io.WriteString` | `w->write_text(s)` | a `string`, a `slice<const char>`, a literal or a `std::string_view` |
| `io.LimitReader`, `io.TeeReader`, `io.MultiReader`, `io.MultiWriter`, `io.Discard` | `limit_reader`, `tee_reader`, `multi_reader`, `multi_writer`, `discard()` | |
| `io.EOF`, `io.ErrUnexpectedEOF` | a read of 0; `errc::unexpected_eof` | the end is not an error |
| `bytes.Buffer` | `buffer` | a reader and a writer over a vector |
| `bufio.Reader`, `ReadString('\n')`, `ReadLine`, `Peek` | `buffered_reader`, `read_line()`, `read_until(c)`, `peek(n)` | a line is a `string`, without its terminator |
| `bufio.Scanner`, `Scan`, `Text`, `Err` | `for (auto line : r->lines())`, `r->last_error()` | `set_max_line` is `Scanner.Buffer`'s bound |
| `bufio.Writer`, `Flush` | `buffered_writer`, `flush()` | the destructor does not flush |
| `os.File`, `Open`, `Create`, `OpenFile`, `NewFile` | `file`, `open(p)`, `create(p)`, `open(p, flags, perm)`, `from_fd(fd)` | one class for every descriptor |
| `ReadAt`, `WriteAt`, `Seek`, `Sync`, `Truncate`, `Stat`, `Chmod` | `read_at`, `write_at`, `seek`, `sync`, `truncate`, `stat`, `chmod` | |
| `os.ReadFile`, `os.WriteFile` | `read_file(p)`, `read_text(p)`, `write_file(p, data)`, `append_file(p, data)` | |
| `os.Pipe` | `pipe()` | both ends non-blocking, on the reactor |
| `os.CreateTemp`, `os.MkdirTemp`, `os.TempDir` | `temp_file(dir, pattern)`, `temp_dir(dir, pattern)`, `temp_path()` | |
| `os.Stat`, `os.Lstat`, `fs.FileInfo`, `fs.FileMode` | `stat(p)`, `lstat(p)`, `file_info`, `permissions` | |
| `os.Mkdir`, `os.MkdirAll`, `os.Remove`, `os.RemoveAll`, `os.Rename`, `os.Symlink`, `os.Readlink`, `os.Chmod`, `os.Chtimes` | `mkdir`, `mkdir_all`, `remove`, `remove_all`, `rename`, `symlink`, `read_link`, `chmod`, `set_modified` | `copy_file` has no Go counterpart |
| `os.ReadDir`, `fs.DirEntry`, `filepath.WalkDir`, `fs.SkipDir` | `read_dir(p)`, `dir_entry`, `walk_dir(root, f)`, `walk_action::skip_dir` | |
| `filepath.Clean`, `Join`, `Base`, `Dir`, `Ext`, `Split`, `SplitList`, `IsAbs`, `Abs`, `Rel`, `Match`, `Glob`, `FromSlash`, `ToSlash` | `path::` the same names, `stem` added | `?` and `[...]` match code points |
| `os.Args`, `os.Getenv`, `LookupEnv`, `Setenv`, `Unsetenv`, `Environ`, `ExpandEnv` | `args()`, `getenv()` (an `optional`: set or not), `setenv`, `unsetenv`, `environ()`, `expand_env` | |
| `os.Getwd`, `Chdir`, `UserHomeDir`, `UserCacheDir`, `UserConfigDir`, `Executable`, `Hostname`, `Getpid`, `Exit` | `working_dir`, `chdir`, `home_dir`, `cache_dir`, `config_dir`, `executable`, `hostname`, `pid`, `exit` | |
| `os.Stdin`, `os.Stdout`, `os.Stderr` | `stdin()`, `stdout()`, `stderr()` | a `tracked_ptr<file>` per call, the descriptor never closed by it |
| `exec.Command`, `Cmd.Run`, `Start`, `Wait`, `Output`, `CombinedOutput`, `StdinPipe`, `StdoutPipe`, `StderrPipe`, `CommandContext`, `WaitDelay` | `command(name, args...)`, `run`, `start`, `wait`, `output`, `combined_output`, `stdin_pipe`, `stdout_pipe`, `stderr_pipe`, the `stop` token, `wait_delay` | `posix_spawn`; the wait on the reactor (`exited(pid)`), no thread per child |
| `os.Process`, `os.ProcessState`, `exec.LookPath`, `exec.ExitError` | `process`, `process_state`, `look_path`, `errc::exit_status` with `cmd.state` | |
| the goroutine that blocks in `Read` | `co_await f->async_read(b)` | the pool for a regular file, the reactor for a pipe or a socket |
