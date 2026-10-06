#!/usr/bin/env python3
# The vectors of tests/txt/html_stencil.cpp: templates in every context of
# Go's html/template, rendered by Go (tools/html_stencil_oracle.go) and by
# the library (tools/html_stencil_driver.cpp) over the same data. A case goes
# into the vectors when both agree (both refusing counts); the others are
# written to <work>/disagree.txt, to be read. Not part of the build:
#
#   tools/html_stencil_vectors.py <oracle> <driver> <work dir> > tests/txt/html_stencil_vectors.h
import os
import random
import subprocess
import sys

R = random.Random(int(os.environ.get("SEED", "20261006")))
ORACLE, DRIVER, WORK = sys.argv[1], sys.argv[2], sys.argv[3]
os.makedirs(WORK, exist_ok=True)

# a context: the text around one field, F the field
CONTEXTS = [
    "F", "<p>F</p>", "<title>F</title>", "<textarea>F</textarea>", '<a title="F">', "<a title='F'>", "<a title=F>",
    "<a title=xF>", '<a href="F">', "<a href='F'>", "<a href=F>", '<a href="/x/F">', '<a href="/x?q=F">',
    '<a href="#F">', '<a href="?F">', '<a href="http://x.com/F?a=F#F">', '<img src="F">', '<img srcset="F">',
    '<a data-href="F">', '<a my:href="F">', '<a xmlns:title="F">', '<a data-title="F">', '<form action="F">',
    "<script>var x = F;</script>", '<script>var x = "F";</script>', "<script>var x = 'F';</script>",
    "<script>var x = /F/;</script>", "<script>x = `aFb`;</script>", "<script>x = `a${F}b`;</script>",
    "<script>if (a) return F;</script>", "<script>var x = a / F;</script>", "<script>x = F / 2</script>",
    "<script>// F\nx = 1</script>", "<script>/* F */ x</script>", '<script type="text/template">F</script>',
    '<script type="application/json">F</script>', '<script type="module">F</script>',
    '<a onclick="f(F)">', "<a onclick='f(\"F\")'>", '<a onmouseover="x = F">', '<a onclick=F>',
    "<style>p { color: F }</style>", '<style>p { content: "F" }</style>', "<style>p { content: 'F' }</style>",
    "<style>p { background: url(F) }</style>", "<style>p { background: url('F') }</style>",
    '<style>p { background: url("/a?F") }</style>', "<style>/* F */ p {}</style>", '<a style="color: F">',
    '<a style="background: url(F)">', "<a style='content: \"F\"'>", "<!-- F -->x", "<a F>", '<a F="1">',
    '<a href="x" F>', "a < b F", "<p>a <F", "<a href=\"&quot;F\">", '<a onclick="x = &quot;F&quot;">',
    '<a href="javascript:F">', "<svg><style>F</style></svg>", "<A HREF=\"F\">", '<input value=F checked>',
    "<br/F>", "<script>var s = \"</script>\"; F</script>", "<p F=\"x\">", "<a href=\"F\" title=\"F\">F</a>",
    "<!DOCTYPE html><p>F", "<?x?>F", "<a title=\"\nF\">", "<textarea>F</TEXTAREA>F", "<style>F</style>",
    "<script>F</script>", "<script>x = {a: F}</script>", "<script>x = [F, F]</script>",
]
FIELDS = ["{{ .s }}", "{{ .i }}", "{{ .d }}", "{{ .b }}", "{{ .n }}", "{{ .missing }}",
          "{{ .s | safe_html }}", "{{ .s | safe_url }}", "{{ .s | safe_js }}", "{{ .s | safe_css }}",
          "{{ .s | safe_attr }}"]
STRINGS = ["left", "O'Reilly: How are <i>you</i>?", "javascript:alert(1)", "a b", "", "\"><script>x</script>",
           "red", "#fff", "10px", "http://x.com/a b?c=d&e", "mailto:a@b", "x' onmouseover='y", "expression(x)",
           "</script>", "a\nb", "ąę €", "${x}", "`", "\\", "1.5em", "50%", "data:x", "/rel", "?q=1", "a.png 2x",
           "onclick", "title", "Data", "a\u2028b", "\x00", "+", "{}", "-moz-binding", "a;b", "url(x)"]
INTS = [0, 1, -5, 42, 1234567]
DOUBLES = [0.5, 3.14159, 88.0, 1e21, 1e-7, -2.5]


def esc(s):
    return s.replace("\\", "\\\\").replace("\t", "\\t").replace("\n", "\\n").replace(";", "\\;").replace("=", "\\=") \
        .replace("\x00", "\\0")


def data_spec():
    d = {"s": ("s", R.choice(STRINGS)), "i": ("i", R.choice(INTS)), "d": ("d", repr(R.choice(DOUBLES))),
         "b": ("b", R.choice(["0", "1"])), "n": ("n", "")}
    return ";".join("%s=%s:%s" % (k, t, esc(str(v))) for k, (t, v) in d.items())


def fill(ctx, field):
    return ctx.replace("F", field)


cases = []
seen = set()
for ctx in CONTEXTS:
    for field in FIELDS:
        for _ in range(3):
            t = fill(ctx, field)
            d = data_spec()
            if (t, d) not in seen:
                seen.add((t, d))
                cases.append((t, d))
# a list or an object as a script's value (in any other context Go writes its fmt form, this its own)
for ctx in ["<script>var x = F;</script>", '<a onclick="f(F)">', "<script>x = [F, F]</script>",
            '<script type="application/json">F</script>']:
    for field in ["{{ .obj }}", "{{ .list }}"]:
        cases.append((fill(ctx, field), data_spec()))
# branches, loops and withs around contexts (inside a range the dot is the element, inside a with the object)
SHAPES = [
    "{{ if .b }}X{{ else }}Y{{ end }}", "{{ if .b }}X{{ end }}", "{{ range .list }}X{{ end }}",
    "{{ with .obj }}X{{ end }}", "{{ range .list }}<li title=\"{{ . }}\">{{ . }}</li>{{ end }}",
    "{{ with .obj }}<a href=\"/{{ .k }}\">{{ .k }}</a>{{ end }}",
]
for _ in range(900):
    shape = R.choice(SHAPES)
    a, b = R.choice(CONTEXTS), R.choice(CONTEXTS)
    inner = "{{ . }}" if "range" in shape else "{{ .k }}" if "with" in shape else None
    fx = inner or R.choice(FIELDS)
    fy = inner or R.choice(FIELDS)
    t = shape.replace("X", fill(a, fx)).replace("Y", fill(b, fy))
    t = R.choice(["", "<p>", '<a href="', "<script>", "<a title='"]) + t if R.random() < 0.2 else t
    d = data_spec()
    if (t, d) not in seen:
        seen.add((t, d))
        cases.append((t, d))

# a soup of HTML pieces and fields: the transitions in orders nobody wrote by hand
PIECES = ["<p>", "</p>", "<a ", "href=", "title=", "onclick=", "style=", "src=", "srcset=", '"', "'", ">", " ", "/",
          "<script>", "</script>", "<style>", "</style>", "<title>", "</title>", "<textarea>", "</textarea>",
          "<!--", "-->", "x", "?", "#", "&amp;", "&quot;", "=", "/*", "*/", "//", "\n", "(", ")", "{", "}", "`", "${",
          "url(", "a/b", "return ", "</", "<b>", "javascript:", ";", ":", "<", "<!DOCTYPE html>", "<svg>",
          '<script type="text/plain">', "data-x=", "a", "1", "+", "-", "[", "]", "\\", "%"]
for _ in range(int(os.environ.get("SOUP", "4000"))):
    parts = []
    for _ in range(R.randint(2, 14)):
        parts.append(R.choice(FIELDS) if R.random() < 0.25 else R.choice(PIECES))
    t = "".join(parts)
    d = data_spec()
    if (t, d) not in seen:
        seen.add((t, d))
        cases.append((t, d))
lines = "".join("%s\t%s\n" % (esc(t), d) for t, d in cases)
go = subprocess.run([ORACLE], input=lines.encode(), capture_output=True, check=True).stdout.decode().split("\n")
ours = subprocess.run([DRIVER], input=lines.encode(), capture_output=True, check=True).stdout.decode().split("\n")


def unesc(s):
    out, i = [], 0
    while i < len(s):
        if s[i] == "\\" and i + 1 < len(s):
            c = s[i + 1]
            out.append("\t" if c == "t" else "\n" if c == "n" else c)
            i += 2
        else:
            out.append(s[i])
            i += 1
    return "".join(out)


agree, disagree = [], []
for (t, d), x, y in zip(cases, go, ours):
    if x == y:
        agree.append((t, d, None if x == "ERROR" else unesc(x)))
    else:
        disagree.append((t, d, x, y))
with open(os.path.join(WORK, "disagree.txt"), "w") as f:
    for t, d, x, y in disagree:
        f.write("%r | %s\n  go   %s\n  ours %s\n" % (t, d, x, y))


def c_literal(s):
    out = ['"']
    for ch in s.encode():
        if ch == 0x22:
            out.append('\\"')
        elif ch == 0x5C:
            out.append("\\\\")
        elif 0x20 <= ch < 0x7F and ch != 0x3F:
            out.append(chr(ch))
        else:
            out.append("\\%03o" % ch)
    out.append('"')
    return "".join(out)


print("//------------------------------------------------------------------------------")
print("// SGCL: a C++20 application platform")
print("// Copyright (c) 2022-2026 Sebastian Nibisz")
print("// SPDX-License-Identifier: Apache-2.0")
print("//------------------------------------------------------------------------------")
print("// Generated by tools/html_stencil_vectors.py from Go's html/template: do not")
print("// edit. Templates, their data (tests/txt/html_stencil_data.h) and the page Go")
print("// writes; page null where Go refuses the template.")
print("#pragma once")
print()
print("struct HtmlStencilVector {")
print("    const char* source;")
print("    const char* data;")
print("    const char* page;")
print("    unsigned page_size;   // the page may hold a NUL")
print("};")
print()
print("inline const HtmlStencilVector HtmlStencilVectors[] = {")
for t, d, p in agree:
    print("    {%s, %s, %s, %d}," % (c_literal(t), c_literal(d), "nullptr" if p is None else c_literal(p),
                                0 if p is None else len(p.encode())))
print("};")
sys.stderr.write("%d cases: %d agree, %d disagree\n" % (len(cases), len(agree), len(disagree)))
