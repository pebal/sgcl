#!/usr/bin/env python3
# The vectors of tests/txt/html.cpp: documents parsed by gumbo-parser 0.13
# (tools/html_oracle.c) and by the library (tools/html_driver.cpp), the trees
# compared in html5lib's format. A case goes into the vectors when both agree;
# the others are written to <work>/disagree.txt, to be read: where gumbo
# lags the current standard the library follows the standard. Not part of the
# build:
#
#   tools/html_vectors.py <oracle> <driver> <work dir> > tests/txt/html_vectors.h
import os
import random
import subprocess
import sys

R = random.Random(int(os.environ.get("SEED", "20261006")))
ORACLE, DRIVER, WORK = sys.argv[1], sys.argv[2], sys.argv[3]
os.makedirs(WORK, exist_ok=True)

HAND = [
    "", "Test", "<p>One<p>Two", "Line1<br>Line2<br>Line3", "<html>", "<head>", "<body>", "<html><head></head><body></body></html>",
    "<p><b><i><u></p> <p>X", "<p><b><i><u></p>\n<p>X", "<b><p>Bold </b> Not bold</p>\nAlso not bold.",
    "<html>\n<b>\n<p>\n</b>\n<p>\n</html>", "<a><p>X<a>Y</a>Z</p></a>", "<b><button>foo</b>bar", "<!DOCTYPE html><span><button>foo</span>bar",
    "<p><b><div><marquee></p></b></div>X", "<script><div></script></div><title><p></title><p><p>", "<!--><div>--<!-->", "<p><hr></p>",
    "<select><b><option><select><option></b></select>X", "<a><table><td><a><table></table><a></tr><a></table><b>X</b>C<a>Y",
    "<a X>0<b>1<a Y>2", "<!-----><font><div>hello<table>excite!<b>me!<th><i>please!</tr><!--X-->",
    "<!DOCTYPE html><li>hello<li>world<ul>how<li>do</ul>you</body><!--do-->", "<!DOCTYPE html>A<option>B<optgroup>C<select>D</option>E",
    "<", "<#", "</", "</#", "<?", "<?#", "<!", "<!#", "<?COMMENT?>", "<!COMMENT>", "</ COMMENT >", "<?COM--MENT?>", "<!COM--MENT>",
    "</ COM--MENT >", "<!DOCTYPE html><style> EOF", "<!DOCTYPE html><script> <!-- </script> --> </script> EOF",
    "<b><p></b>TEST", "<p id=a><b><p id=b></b>TEST", "<b id=a><p><b id=b></p></b>TEST", "<!DOCTYPE html><title>U-test</title><body><div><p>Test<u></p></div></body>",
    "<!DOCTYPE html><font><table></font></table></font>", "<font><p>hello<b>cruel</font>world", "<b>Test</i>Test", "<b>A<cite>B<div>C",
    "<b>A<cite>B<div>C</cite>D", "<b>A<cite>B<div>C</b>D", "", "<DIV>", "<DIV> abc", "<DIV> abc <B>", "<DIV> abc <B> def <I>",
    "<DIV> abc <B> def <I> ghi <P>", "<DIV> abc <B> def <I> ghi <P> jkl", "<DIV> abc <B> def <I> ghi <P> jkl </B>",
    "<DIV> abc <B> def <I> ghi <P> jkl </B> mno", "<DIV> abc <B> def <I> ghi <P> jkl </B> mno </I>",
    "<DIV> abc <B> def <I> ghi <P> jkl </B> mno </I> pqr", "<DIV> abc <B> def <I> ghi <P> jkl </B> mno </I> pqr </P>",
    "<DIV> abc <B> def <I> ghi <P> jkl </B> mno </I> pqr </P> stu", "<test attribute---------------------------------------------->",
    "<a href=\"blah\">aba<table><a href=\"foo\">br<tr><td></td></tr>x</table>aoe", "<a href=\"blah\">aba<table><tr><td><a href=\"foo\">br</td></tr>x</table>aoe",
    "<table><a href=\"blah\">aba<tr><td><a href=\"foo\">br</td></tr>x</table>aoe", "<a href=a>aa<marquee>aa<a href=b>bb</marquee>aa",
    "<wbr><strike><code></strike><code><strike></code>", "<!DOCTYPE html><spacer>foo", "<title><meta></title><link><title><meta></title>",
    "<style><!--</style><meta><script>--><link></script>", "<head><meta></head><link>", "<table><tr><tr><td><td><span><th><span>X</table>",
    "<body><body><base><link><meta><title><p></title><body><p></body>", "<textarea><p></textarea>", "<p><image></p>", "<a><table><a></table><p><a><div><a>",
    "<head></p><meta><p>", "<head></html><meta><p>", "<b><table><td><i></table>", "<b><table><td></b><i></table>X", "<h1>Hello<h2>World",
    "<a><p>X<a>Y</a>Z</p></a>", "<b><button>foo</b>bar", "<!DOCTYPE html><span><button>foo</span>bar", "<p><b><div><marquee></p></b></div>X",
    "<script><div></script></div><title><p></title><p><p>", "<!DOCTYPE html><table><tr><td>a</td><td>b</td></tr></table>", "<table><caption>x<td>y",
    "<table><colgroup><col><col></colgroup><tbody><tr><td>1</td></tr></tbody></table>", "<table><tr>x<td>y</table>", "<table> x </table>",
    "<table><tr><td><svg><desc><td></desc><circle></svg></td></tr></table>", "<table><form><input type=hidden><input></form>",
    "<table><td><select><option>1<td>2</select>", "<select><option>a<optgroup><option>b</optgroup><hr><option>c</select>",
    "<select><input>x", "<select><textarea>x", "<select><keygen>x", "<select><script>a</script></select>",
    "<template><td>a</td></template>", "<template><tr><td>a</td></tr></template>", "<template><col></template>", "<template><caption></template>",
    "<template><template><b>x</template></template>", "<head><template><meta></template></head>", "<template></template><body>",
    "<table><template><tr><td>a</template></table>", "<frameset><frame><noframes>x</noframes></frameset>", "<frameset></frameset>x",
    "<body><frameset>", "<p><frameset>", "<div><frameset>", "<!DOCTYPE html><frameset><frame></frameset><!--x-->",
    "<svg><path d=x/><circle/></svg>", "<svg><foreignObject><b>x</b></foreignObject><g/></svg>", "<math><mi>x</mi><mo>+</mo><annotation-xml encoding=text/html><p>a</p></annotation-xml></math>",
    "<math><annotation-xml><svg><p>x</svg></annotation-xml></math>", "<svg><title><b>x</b></title></svg>", "<svg><b>x</svg>", "<svg><font color=red>x</svg>",
    "<svg><font>x</font></svg>", "<math><mtext><b>x</b></mtext><mglyph/></math>", "<svg xlink:href=a xml:lang=en xmlns:xlink=b definitionurl=c>",
    "<math definitionurl=x xlink:href=y>", "<svg><![CDATA[a<b]]></svg>", "<p><![CDATA[x]]>", "<svg><clippath><lineargradient></svg>",
    "<svg viewbox=1 preserveaspectratio=2 attributename=3>", "<svg></p><p>x</svg>", "<svg></br>x</svg>", "<svg><script>a</script></svg>",
    "&amp;&lt;&gt;&quot;&#39;&#x41;&#65;&#0;&#xD800;&#x110000;&#128;&#x9F;", "&notin; &notit; &not &ampx &amp;x &AElig &AElig; &noexist;",
    "<a href='&amp;&ampx&amp=&lt;'>x</a>", "<a title=&notit>x</a>", "<p title=\"&#x26;\">x", "&#", "&#x", "&#;", "&x", "&;",
    "a\0b", "<p>\0</p>", "<table>\0</table>", "<svg>\0</svg>", "<select>\0</select>", "<title>\0</title>", "<script>\0</script>",
    "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\"><p>x",
    "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\"><table><p>x", "<!DOCTYPE html SYSTEM \"about:legacy-compat\">",
    "<!doctype HTML>", "<!DOCTYPE>", "<!DOCTYPE html x>", "<!DOCTYPE html public>", "<!DOCTYPE html PUBLIC 'a' 'b'>",
    "<!DOCTYPE html SYSTEM 'x'>", "<!DOCTYPE html PUBLIC \"x\">", "<!DOCTYPE foo>", "<!--a--!>b", "<!--a--!-->b", "<!-->", "<!--->",
    "<!-- a -- b -->", "<!--<!--x-->", "<p>a<!--b-->c", "<script>a<!--b<script>c</script>d</script>e", "<script><!--<script></script>--></script>x",
    "<script><!--<script>--></script>x", "<style></style>x", "<xmp><p></xmp>", "<iframe><p></iframe>", "<noembed><p></noembed>", "<noscript><p></noscript>",
    "<head><noscript><link></noscript></head>", "<plaintext><p></plaintext>", "<pre>\nx</pre>", "<pre>\n\nx</pre>", "<listing>\nx</listing>", "<textarea>\nx</textarea>",
    "<ul><li>a<li>b</ul>", "<ol><li>a<ol><li>b</ol></ol>", "<dl><dt>a<dd>b<dt>c</dl>", "<ruby>a<rb>b<rt>c<rp>d<rtc>e</ruby>", "<p><ul></ul>",
    "<form><form>x</form>", "<form><div></form>x", "<button><button>", "<a><a>", "<nobr><nobr>x", "<object><p>x</object>", "<applet><a></applet>",
    "<marquee>x<p>y</marquee>", "<h1><h2>x</h1>", "<h1>x</h2>", "<p>x</h1>", "</p>", "</br>", "</div>", "</body>x", "</html>x", "<body></html>x<!--y-->",
    "<html a=b><body c=d><html e=f><body g=h>", "<input type=hidden><input type=HIDDEN>", "<image>", "<isindex>", "<hr/><br/><img/><input/>",
    "<div/>x", "<span/>x", "<a b=1 b=2 c>", "<a b='1'c=\"2\">", "<a =b>", "<a b=>", "<a\"b>", "<a b\"c=d>", "<a b='c>", "<!DOCTYPE html><p><table>x",
    "<p><table>x", "<search>a<p>b</search>", "<dialog><p>x</dialog>", "<main><p>x</main>", "<details><summary>a</summary>b</details>",
    "<b><i><a><s><tt><div></b>x", "<b><b><b><b>x</b></b>y", "<b>1<b>2<b>3<b>4<b>5</b></b></b></b></b>6", "<b class=x><b class=x><b class=x><b class=x><p>y",
    "<a><svg><a></a></svg></a>", "<table><tr><td><table><tr><td>x</td></tr></table></td></tr></table>", "<table><tbody><tr></tbody><tr></table>",
    "<table><thead><tfoot><tbody>", "<table><td></th>x", "<table></tr><tr></table>", "<table><caption><table></caption>", "<colgroup><col>",
    "<tr><td>x", "<td>x", "<th>x", "<caption>x", "<col>x", "<frame>", "<head><base><link><meta><title>t</title><style>s</style><script>j</script></head>",
    "x<head>y", "<html> <head> </head> <body> </body> </html> ", "<!DOCTYPE html>  <html>  <head>  </head>  <body>  </body>  </html>  ",
    "<body>\n<script>x</script>\n</body>\n", "<p>\r\na\rb</p>", "\xef\xbb\xbfx", "\xff\xfe<p>", "<p>\xe2\x80</p>", "<p>\xed\xa0\x80</p>",
    "<svg><desc><svg><b>x</svg></desc></svg>", "<math><mi><svg><foreignObject><p>x</math>", "<div><svg><circle></div>x",
    "<svg><foreignObject><svg><foreignObject><p>x</p></foreignObject></svg></foreignObject></svg>", "<table><svg><td>x</svg>",
    "<select><svg>x</svg></select>", "<template><svg><td>x</svg></template>", "<frameset><svg>", "<p><math><mtext><p>x",
]

PIECES = ["<p>", "</p>", "<b>", "</b>", "<i>", "</i>", "<a href=x>", "</a>", "<div>", "</div>", "<table>", "</table>", "<tr>", "</tr>",
          "<td>", "</td>", "<th>", "<tbody>", "<caption>", "<colgroup>", "<col>", "<select>", "</select>", "<option>", "<optgroup>",
          "<svg>", "</svg>", "<math>", "</math>", "<foreignObject>", "<mi>", "<annotation-xml encoding=text/html>", "<desc>",
          "<template>", "</template>", "<script>", "</script>", "<style>", "</style>", "<title>", "</title>", "<textarea>",
          "</textarea>", "<pre>", "<li>", "<ul>", "</ul>", "<dd>", "<dt>", "<h1>", "</h1>", "<h2>", "<form>", "</form>", "<button>",
          "<nobr>", "<font color=red>", "<frameset>", "<frame>", "<noframes>", "<body>", "</body>", "<html>", "</html>", "<head>",
          "</head>", "<br>", "</br>", "<hr>", "<img>", "<input type=hidden>", "<input>", "<!--c-->", "<!DOCTYPE html>", "x", " ",
          "\n", "&amp;", "&notit;", "\\0", "<![CDATA[y]]>", "<marquee>", "<object>", "<rb>", "<rt>", "<ruby>", "<plaintext>",
          "<xmp>", "<iframe>", "<noscript>", "<image>", "<search>", "<s>", "<em>", "<u>"]

cases = list(HAND)
for _ in range(int(os.environ.get("SOUP", "3000"))):
    cases.append("".join(R.choice(PIECES) for _ in range(R.randint(1, 16))))


def esc(s):
    return s.replace("\\", "\\\\").replace("\n", "\\n").replace("\r", "\\r").replace("\t", "\\t").replace("\0", "\\0")


lines = "".join(esc(c) + "\n" for c in cases).encode("utf-8", "surrogateescape")
go = subprocess.run([ORACLE], input=lines, capture_output=True, check=True).stdout.decode("utf-8", "replace")
ours = subprocess.run([DRIVER], input=lines, capture_output=True, check=True).stdout.decode("utf-8", "replace")
gt = go.split("#end\n")
ot = ours.split("#end\n")
def merge_text(tree):
    # gumbo leaves adjacent text nodes apart where the standard joins them (a
    # character inserted beside a text node is appended to it): joined here
    out = []
    for line in tree.split("\n"):
        if out and line.startswith("| ") and out[-1].startswith("| "):
            a, b = out[-1], line
            ia = len(a) - len(a[2:].lstrip(" ")) - 2
            ib = len(b) - len(b[2:].lstrip(" ")) - 2
            if ia == ib and a[2 + ia:].startswith('"') and a.endswith('"') and b[2 + ib:].startswith('"'):
                out[-1] = a[:-1] + b[2 + ib + 1:]
                continue
        out.append(line)
    return "\n".join(out)


agree, disagree, merged = [], [], 0
for c, a, b in zip(cases, gt, ot):
    if a == b:
        agree.append((c, a))
    elif merge_text(a) == b:
        agree.append((c, b))   # the standard's tree; gumbo split a text node
        merged += 1
    else:
        disagree.append((c, a, b))
with open(os.path.join(WORK, "disagree.txt"), "w") as f:
    for c, a, b in disagree:
        f.write("=== %r\n--- gumbo\n%s--- ours\n%s" % (c, a, b))


def c_literal(s):
    out = ['"']
    for ch in s.encode("utf-8", "surrogateescape"):
        if ch == 0x22:
            out.append('\\"')
        elif ch == 0x5C:
            out.append("\\\\")
        elif 0x20 <= ch < 0x7F and ch != 0x3F:
            out.append(chr(ch))
        else:
            out.append("\\%03o" % ch)
    out.append('"')
    s = "".join(out)
    return s


print("//------------------------------------------------------------------------------")
print("// SGCL: a C++20 application platform")
print("// Copyright (c) 2022-2026 Sebastian Nibisz")
print("// SPDX-License-Identifier: Apache-2.0")
print("//------------------------------------------------------------------------------")
print("// Generated by tools/html_vectors.py from gumbo-parser 0.13: do not edit.")
print("// Documents and their trees in html5lib's test format.")
print("#pragma once")
print()
print("struct HtmlVector {")
print("    const char* document;")
print("    unsigned size;   // the document may hold a NUL")
print("    const char* tree;")
print("};")
print()
print("inline const HtmlVector HtmlVectors[] = {")
for c, t in agree:
    print("    {%s, %d, %s}," % (c_literal(c), len(c.encode("utf-8", "surrogateescape")), c_literal(t)))
print("};")
sys.stderr.write("%d cases: %d agree (%d once gumbo's split text nodes are joined), %d disagree\n"
                 % (len(cases), len(agree), merged, len(disagree)))
