# How a page of the documentation is written

The rules every page under `docs/` and the root `README.md` follow. They are complete: a change to the
documentation is checked against every point here, not against the one that prompted it. An agent that
touches a page reads this file first and leaves the page satisfying all of it.

## 1. The order of a page

1. The title: the fully qualified name, `# sgcl::net::http::client`; a page of a header without a class of its
   own the same way, `# sgcl::txt::bidi`. Never the name without `sgcl::`.
2. The names block: a `cpp` block with the include line and the declarations, the class header first and the
   module header as the remark — `#include "sgcl/net/http/client.h"   // or "sgcl/net/http/http.h"`.
   No program, no fragment of a program, sits between the title and this block or between this block and
   the description.
3. The description: what the type is, how it differs from `std` and Go, and its rules (`## Rules`: where an
   object may live, what it may hold, what waits and what does not). Prose, no code fragments.
4. `## Members`: every public member, grouped, each with its signature in a `cpp` block and one or two
   sentences; a data member (a public field, an options struct's field, a setting) is listed with what it
   means and its default.
5. `## Example` (or `## Examples`): the programs, at the end of the page, before `## See also`.
6. `## See also`.

A nested type of a class has a page of its own named with a hyphen, `json-reader.md` for `encoding::json::reader`,
`xml-writer.md` for `encoding::xml::writer`; a directory is for a namespace (`net/http/`), not for a class.

A page about a concept rather than a class of its own (ECDSA over p256 and p384, the hash ids) describes the
declarations it covers under its rules and has no `## Members`; the members are on the pages of the classes.

A README of a module is a guide (what the classes are, the rules, what to reach for) and may put a short
program under a heading of its own; it still never opens with one.

## 2. A program

- Every example is a complete program with `int main`, short, about one thing; a page has several short
  programs rather than one long one. A GET and a POST may share one when it stays under a screen.
- It compiles and runs as pasted, from the root of the tree, and prints what the page says (section 3).
- Includes: one header per module the program uses, the module's umbrella header — `sgcl/core/core.h`,
  `sgcl/io/io.h`, `sgcl/net/net.h`, `sgcl/net/http/http.h`, `sgcl/net/tls.h`, `sgcl/concurrent/concurrent.h`,
  `sgcl/async/async.h`, `sgcl/time/time.h`, `sgcl/encoding/encoding.h`, `sgcl/hash/hash.h`,
  `sgcl/crypto/crypto.h`, `sgcl/compress/compress.h`, `sgcl/codec/codec.h`, `sgcl/slog/slog.h`,
  `sgcl/math/math.h`, `sgcl/txt/txt.h`, `sgcl/immutable/immutable.h`. Never `sgcl/sgcl.h`, never a class
  header (`sgcl/io/print.h`, `sgcl/encoding/base64.h`), never a header of a module the program does not
  name. Core is a module like the others: `sgcl/core/core.h` when the program uses `range`, `thread`,
  `map` or another name of core that the other headers do not bring. A module header brings only what
  the module itself needs, so every used module is named.
- The include lines of sgcl stand in alphabetical order of their paths (`sgcl/async/async.h`, `sgcl/core/core.h`,
  `sgcl/io/io.h`, `sgcl/net/http/http.h`, `sgcl/net/net.h`…); a `<…>` header of the standard library after them.
- `using namespace sgcl;` after the includes, and no directive of a module of sgcl: every other module's name is
  written with its module (`net::http::client`, `encoding::base64`, `io::read_file`).
  `using namespace std::chrono_literals;` is allowed where a program writes `5s` or `100ms`.
- A counted loop is `for (int i : range(n))`, never `for (int i = 0; i < n; ++i)`.
- Nothing the compiler converts by itself is written: no `*` on an `expected` passed on, no `slice<>(x)`,
  no `as_slice()`; `sgcl::tracked_ptr p = make_tracked<T>()`, not the type twice.
- A variable is not named after its type; a value is named by its type where `auto` would need a `*`.
- `force_collect()` appears only marked as optional, for demonstration.
- Comments: an explanatory comment stays short, two spaces after the code, or on its own line above the
  statement when the line would pass 100 columns. No comment repeats what the line prints when the page
  has an output block under the program (section 3). No comment is aligned to a far column. No line of a
  program is longer than 100 columns.
- A program that writes files does so in the checker's temporary directory (`--tmp` for the page in
  `tools/run_blocks.pages`), never in the tree.

## 3. The output of a program

Directly under a program, one of three things, by what the program prints:

- **A determined result** — the label `Output:` and a `text` block with the exact print. The checker compares
  it literally.
- **A result that varies but is not the user's data** (the time, the times of a cycle, ids, the order threads
  interleave in, the order of a hash map, random bytes, the memory ceiling, the number of cores) — the
  label `Sample output:` and a `text` block with one run's print. The checker compiles and runs the program
  and does not compare. No sentence above the block says it varies: the label says it.
- **A result that may show the user's data** (a listing of a directory, home paths, the hostname, the
  environment) — no output block at all. The checker compiles and runs the program.

The label is exactly `Output:` or `Sample output:` on its own line, followed by a blank line and a
```` ```text ```` block. No other label, no note inside the label. A program's answer to a request
(a server) is the label `A request to it:` with a `text` block of the `curl` lines and their answers, and
`Its output:` for what the server itself prints.

## 4. What the checker needs

`tools/run_blocks.py <page> --strict` compiles every program of the page with `-Wall
-Wno-unused-variable`, runs it, and compares its print with the `Output:` block. A page needs no
arguments: what its programs need — a fresh directory (`--tmp`), a local file server in place of a URL
(`--serve URL=FILE`), an echo server (`--echo URL`), a local TLS server (`--tls HOST:PORT`), a program not
to run (`--skip N`) — is listed for the page in `tools/run_blocks.pages`, one line per page, never in a
comment on the page. Under `--strict`, a page passes with `bad 0` and `unchecked 0`; a program left unread
by the checker (a label it does not know) is `unchecked`, which fails.

## 5. Checking a change

Before a page is handed in:

1. Every point of sections 1–3 holds for the whole page, not only for the lines changed.
2. `tools/run_blocks.py <page> --strict --changed`: `bad 0`, `unchecked 0`, and nothing left in the tree (`git
   status` clean apart from the pages). `--changed` compiles only the programs whose text or answer differs from
   the page in git HEAD: a change of prose, a title, a label or a names block costs no compilation, and a program
   that did not change is never compiled again.
3. `grep` for what the change removed (an old name, `sgcl/sgcl.h` in a program, a class header, a `The
   output:` label, a note in a label) gives 0 over `docs/` and `README.md`.
4. The change touches only what it says: a relabelling changes labels, a comment change changes comments,
   a code change is announced as one. A program's code is not rewritten to make its output stable; the
   output rule of section 3 covers that.
