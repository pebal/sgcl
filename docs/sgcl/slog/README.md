[sgcl](../README.md) › slog

# sgcl::slog

```cpp
#include "sgcl/slog.h"   // namespace sgcl::slog
```

Structured logging: what Go has in `log/slog`. A record is a level, a message and attributes, pairs of a key and a
value that the compiler checks; it is written as a line of text or of JSON, byte for byte as slog writes it, or
handed to a handler of the program. The module depends on [core](../core/README.md), [async](../async/README.md)
(the workers a buffered logger batches per), [io](../io/README.md) (the writer a line goes to, `io::stderr` by
default), [txt](../txt/README.md) (what Unicode says is printable, for the quoting), [time](../time/README.md) (the
time of a record, in the local zone) and [encoding](../encoding/README.md) (a type described by its fields); the
index of the whole interface is [the modules](../README.md).

[debug, info, warn and error](debug.md) write through the [default logger](default_logger.md): text on
`io::stderr`, from `info` up. A [logger](logger.md) of one's own is a handle of one word, made by its constructor
from [options](options.md) — the output, text or JSON, the level, `source`, `utc`, `buffered`, sampling — or from a
writer and a level for text lines, or from a [handler](handler.md) of the program; its verbs write.
[with](logger/with.md) gives a logger that writes its attributes in every record, rendered once when it is made,
and [group](logger/group.md) puts what comes after it in a group. Copies, and the loggers made from one by `with`
and `group`, share its output.

The attributes are pairs, a key and a value: the key a literal (or a `const char*`), the value one of the kinds
below, or a [group](group.md) standing where a key would. An odd count, a key that is not a string, a value of a
kind the module does not take, a character given as a value (a string is given, or an integer for its code): each is
an error of the build, where Go writes `!BADKEY` or `%v` at run time.

## The rules

- **One record, one write.** The line is made whole in plain memory of the thread and written by one `write` to the
  writer, from the thread that logs, with no lock of the module's: the writer takes writes from many threads at
  once, as a file opened for appending, `io::stderr` and a connection do. A `buffered_writer` does not: set
  [options](options.md)`::buffered` instead.
- **Nothing managed per record.** A record allocates nothing on the managed heap: the line is in the thread's plain
  memory, grown once to the largest record; the attributes of `with` were rendered when the logger was made; a
  described type is read through its fields where it lies. A value whose type makes its text as a `string`
  (`to_text`, `to_string`, an `io::error`'s message) costs that string. `tests/slog/heap.cpp` counts it.
- **Below the level, nothing.** A record below the logger's level is one comparison: no line, no text of any value.
  The arguments themselves are computed by the caller, as for any call: an argument that costs to compute goes under
  `if (log.enabled(slog::level::debug))` ([enabled](logger/enabled.md)).
- **A write that fails** is not returned to the caller, as `print`'s is not: [dropped](logger/dropped.md) counts the
  records lost, and the first failure of an output says so in one line on `io::stderr`.
- **Buffered.** `options::buffered` gathers the lines of each worker of the scheduler in a batch of 32 KB of its own,
  written by one write when it is full, at a record of `warn` and up, when the worker goes to sleep, at
  [flush](logger/flush.md) and at exit (`atexit`, `at_quick_exit`). The order across workers holds only within each
  worker; every line has its time. A thread that is no worker shares one batch with the others, written when it is
  full, at a `warn`, at `flush()` and at exit.
- **Handlers of the program.** `slog::logger(h)` (or `options::handler`) gives the records to a handler of the
  program ([handler](handler.md)): any type with `handle(const record&)`, called from every thread that logs. The
  [record](record.md) is a view of the call's stack; [clone](record/clone.md) is a copy to keep.
  [memory](memory.md) keeps them, for tests.
- **Rotation and sending logs elsewhere** are not in the module; a handler of the program does either.
- **What throws.** A record whose write fails throws nothing. What a verb may throw is the program's own: what a
  value's `to_text` or `to_string` throws, what a handler of the program throws, what a writer of the program
  throws. [value](value.md)'s `as_` accessors throw `logic_error` for a value of another kind.

### The kinds of values

| Value | Text | JSON |
|---|---|---|
| `bool`, an integer, `float`, `double` | `true`, `-5`, `1.5`, `1e+06` (Go's `%v`); NaN as `NaN` | `true`, `-5`, `1.5`, `1000000` (json.Marshal's); NaN and ±Inf as slog's error string |
| a literal, `const char*`, `string`, `std::string`, `std::string_view`, a text slice | as it is, or in quotes when it has to be | a string |
| `duration`, a `std::chrono` duration | `1.5s` | nanoseconds, `1500000000` |
| `time::datetime` | `2026-09-28T14:05:01.123+02:00` (milliseconds) | RFC 3339 with the nanoseconds |
| `io::error`, `std::error_code` | its message | a string |
| `std::exception_ptr` | `std::runtime_error: what it said` (null: `<nil>`) | a string (null: `null`) |
| `optional<T>`, `nullptr` | the value, or `<nil>` | the value, or `null` |
| a type with `write_text(char*)` and `MaxText` (`net::ip_address`, `net::endpoint`, `net::ip_network`) or a `format_value` | its text, written into the line | a string |
| a type with `to_text()` or `to_string()` | its text (the string it makes is its cost) | a string |
| a type with `describe` ([field_list](../encoding/field_list.md)) | a group of its fields: `req.id=5 req.path=/a` | an object: `"req":{"id":5,"path":"/a"}` |
| [group](group.md)`("req", "id", id, ...)` | `req.id=5` | `"req":{"id":5}` |

The first kind that matches is taken, in the order of the table. A field of a described type that is a container, a
map, a variant or a `json` is written as Go writes a value of `KindAny`: in text as `%+v` does (`[a b]`,
`map[k:v]`, `{id:5 path:/a}`), in JSON as `json.Marshal` does. A group of no attributes is left out, as slog leaves
it. Groups nest as deep as the program writes them. A described type is followed 64 levels of described types deep,
for one that leads back to itself through a pointer: the level past them is left out, as an empty group is, by both
formats, by `with` and by [clone](record/clone.md) alike. A container is followed 64 levels deep too: `...` past them
in text, json.Marshal's cycle error in JSON.

### The formats

Text is slog's `TextHandler` byte for byte: `time=… level=INFO msg="server started" port=8080`, a text in quotes
when it is empty or holds a space, `=`, `"`, a control, invalid UTF-8 or a code point Unicode does not call
printable, escaped inside as Go's `strconv.Quote` escapes (`\n`, `\x1b`, `\u00a0`, `\xff` for a byte of invalid
UTF-8); a group's name before its keys with a dot. JSON is slog's `JSONHandler` byte for byte: the keys `time`,
`level`, `source`, `msg`, a group an object. The time is the local zone's (`+02:00`), or UTC with
[options](options.md)`::utc` (`Z`): three places of the second in text, the nanoseconds in JSON (trailing zeros
dropped, as Go's `RFC3339Nano`). A level between the named ones is written as Go writes it, from the nearest named
level at or below it: `INFO+2`, `DEBUG-1` ([level](level.md)). `tools/slog_oracle.go` writes the lines Go writes
for the same records, and `tests/slog/oracle.cpp` compares them.

`options::source` adds where the call is: `source=src/main.cpp:42` in text,
`"source":{"function":"int main()","file":"src/main.cpp","line":42}` in JSON — the function as the compiler names
it, the file as it was given to the compiler.

### The collector's log

[collector_log](collector_log.md) makes the collector's own lines (`SGCL_LOG_PRINT_LEVEL`,
[config](../core/config.md#sgcl_log_print_level)) records of a logger, instead of lines on `std::cout`: every one at
`info`, `msg=collector`, the macro's level as `verbosity`, the line of a cycle as its numbers. A line comes from the
collector's thread or from a thread's registration, where nothing managed may be touched: it is copied into a queue
of plain memory there, and written by a thread that may log — a worker on its way to sleep, the next record of any
logger, `flush()`, the exit. Until the call the lines stay on `std::cout`, byte for byte as before.

### From code written for Go

| Go | SGCL |
|---|---|
| `slog.Info("msg", "k", v)` | `slog::info("msg", "k", v)`: the pairs checked by the compiler, no `!BADKEY` |
| `slog.New(slog.NewJSONHandler(w, &slog.HandlerOptions{Level: slog.LevelDebug, AddSource: true}))` | `slog::logger(slog::options{.out = w, .level = slog::level::debug, .json = true, .source = true})`: the options a struct |
| `logger.With("k", v)`, `WithGroup("g")` | `log.with("k", v)`, `log.group("g")` |
| `slog.Group("g", ...)` | `slog::group("g", ...)` |
| `slog.LevelVar` | `slog::level_var` |
| `slog.SetDefault`, `slog.Default` | `slog::set_default`, `slog::default_logger`: atomic |
| a struct as `slog.Any` | a type with `describe`: a group; `%+v` and `json.Marshal` for its containers |
| `HandlerOptions.ReplaceAttr` | none: a handler of the program |
| `slog.Handler` (an interface) | `slog::handler` (a value over any type with `handle`): `with`'s attributes arrive inside the record, as a tree |
| none | `options::buffered`, `options::sample_first`/`sample_then`/`sample_per`, `dropped()`: a batch per worker; zap's sampling |

## Functions

| Function | Header | Description |
|---|---|---|
| [collector_log](collector_log.md) | `collector_log.h` | the collector's lines as records of a logger |
| [debug, info, warn, error](debug.md) | `logger.h` | a record written through the default logger |
| [default_logger](default_logger.md) | `logger.h` | the logger the free functions write through |
| [set_default](set_default.md) | `logger.h` | replaces the default logger, for every thread |

## Classes

| Class | Header | Description |
|---|---|---|
| [attr](attr.md) | `record.h` | an attribute of a record as a handler reads it: its key and value, slog's `Attr` |
| [attrs](attrs.md) | `record.h` | the attributes of a group in order, a range of `attr`: what `value::as_group` returns |
| [group\<A...\>](group.md) | `logger.h` | attributes in a group of their own, slog's `Group` |
| [handler](handler.md) | `handler.h` | any handler of the program held as a value, slog's `Handler` |
| [level_var](level_var.md) | `level.h` | a level changed while the program runs, shared by loggers, slog's `LevelVar` |
| [logger](logger.md) | `logger.h` | what a record is written as, from which level, with which attributes, slog's `Logger` |
| [memory](memory.md) | `memory.h` | a handler that keeps the records, for tests |
| [message](message.md) | `record.h` | the text of a record and where the call is |
| [options](options.md) | `logger.h` | what a logger is made with, slog's `HandlerOptions` and the choice of handler |
| [record](record.md) | `record.h` | a record as a handler reads it, slog's `Record` |
| [value](value.md) | `record.h` | the value of an attribute as a handler reads it, slog's `Value` |

## Enumerations

| Enumeration | Header | Description |
|---|---|---|
| [level](level.md) | `level.h` | the importance of a record, with Go's numbers: `debug`, `info`, `warn`, `error` |
| [value::kind](value-kind.md) | `record.h` | the kind of a value: `null`, `boolean`, `int64`, `string`, `group`... |

## Requirements

What a handler of the program is ([req](req.md), `namespace sgcl::slog::req`).

| Requirement | Description |
|---|---|
| [handler](req/handler.md) | a type with `handle(const record&)`, and `enabled(level)` if it has one |

## See also

- [net::http::server](../net/http/server.md): the access log, records of the default logger
- [io::writer](../io/writer.md): where the lines go
- `tests/slog/logger.cpp`, `tests/slog/oracle.cpp`, `tests/slog/heap.cpp`, `tests/slog/threads.cpp`: the behaviour of
  the module, checked against Go; `tests/slog/boundary.cpp`: its boundaries
- [The modules](../README.md)
