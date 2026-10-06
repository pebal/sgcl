#!/usr/bin/env python3
# The vectors of tests/txt/markdown.cpp: documents — a hand-written corpus over
# every section of CommonMark 0.31 and GitHub's extensions, and random ones
# built of block and inline pieces — written by md4c (tools/markdown_oracle.c)
# and by the library (tools/markdown_driver.cpp), the two HTMLs compared after
# the normalization of CommonMark's own test runner (white space next to block
# tags, attribute order, void tags, entities). Prints a summary on stderr and
# the agreeing cases on stdout:
#
#   tools/markdown_vectors.py <markdown_driver> <markdown_oracle> > tests/txt/markdown_vectors.h
import html
import html.parser
import os
import random
import re
import subprocess
import sys

R = random.Random(int(os.environ.get("SEED", "20261006")))
DRIVER, ORACLE = sys.argv[1], sys.argv[2]
N = int(os.environ.get("N", "1200"))
SHOW = int(os.environ.get("SHOW", "12"))

BLOCK_TAGS = {"article", "header", "aside", "hgroup", "blockquote", "hr", "iframe", "body", "li", "map", "button",
              "object", "canvas", "ol", "caption", "output", "col", "p", "colgroup", "pre", "dd", "progress", "div",
              "section", "dl", "table", "td", "dt", "tbody", "embed", "textarea", "fieldset", "tfoot", "figcaption",
              "th", "figure", "thead", "footer", "tr", "form", "ul", "h1", "h2", "h3", "h4", "h5", "h6", "video",
              "script", "style"}


class Normalizer(html.parser.HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.tokens = []
        self.in_pre = 0

    def handle_starttag(self, tag, attrs):
        attrs = [(k, "" if k in ("checked", "disabled") else (v or "")) for k, v in attrs
                 if not (k == "class" and v and v.startswith("task-list-item"))]
        attrs.sort()
        self.tokens.append(("start", tag, tuple(attrs)))
        if tag == "pre":
            self.in_pre += 1

    def handle_startendtag(self, tag, attrs):
        self.handle_starttag(tag, attrs)
        if tag == "pre":
            self.in_pre -= 1

    def handle_endtag(self, tag):
        self.tokens.append(("end", tag, ()))
        if tag == "pre":
            self.in_pre -= 1

    def handle_data(self, data):
        self.tokens.append(("text", data, self.in_pre > 0))

    def handle_comment(self, data):
        self.tokens.append(("comment", data, ()))

    def handle_decl(self, decl):
        self.tokens.append(("decl", decl, ()))

    def handle_pi(self, data):
        self.tokens.append(("pi", data, ()))

    def unknown_decl(self, data):
        self.tokens.append(("decl", data, ()))


def normalize(h):
    # line breaks next to the tags of lists and quotes (an unclosed comment
    # would otherwise keep them as its text)
    h = re.sub(r"<(li|ul|ol|blockquote)>\n", r"<\1>", h)
    h = re.sub(r"\n</(li|ul|ol|blockquote)>", r"</\1>", h)
    p = Normalizer()
    try:
        p.feed(h)
        p.close()
    except Exception:
        return h
    toks = p.tokens
    # texts joined
    joined = []
    for t in toks:
        if t[0] == "text" and joined and joined[-1][0] == "text":
            joined[-1] = ("text", joined[-1][1] + t[1], joined[-1][2] or t[2])
        else:
            joined.append(t)
    out = []
    for i, t in enumerate(joined):
        if t[0] == "text":
            data = t[1]
            if not t[2]:
                prev = joined[i - 1] if i > 0 else None
                nxt = joined[i + 1] if i + 1 < len(joined) else None
                if prev is None or (prev[0] in ("start", "end") and prev[1] in BLOCK_TAGS):
                    data = data.lstrip()
                if prev is not None and prev[0] == "start" and prev[1] == "input":
                    data = data.lstrip()
                if nxt is None or (nxt[0] in ("start", "end") and nxt[1] in BLOCK_TAGS):
                    data = data.rstrip()
            if data:
                out.append(html.escape(data, quote=False))
        elif t[0] == "start":
            out.append("<" + t[1] + "".join(' %s="%s"' % (k, html.escape(v)) for k, v in t[2]) + ">")
        elif t[0] == "end":
            out.append("</" + t[1] + ">")
        elif t[0] == "comment":
            out.append("<!--" + t[1] + "-->")
        else:
            out.append("<!" + t[1] + ">")
    return "".join(out)


CORPUS = [
    # tabs
    "\tfoo\tbaz\t\tbim\n", "  \tfoo\tbaz\t\tbim\n", "    a\ta\n    ὐ\ta\n", "  - foo\n\n\tbar\n", "- foo\n\n\t\tbar\n",
    ">\t\tfoo\n", "-\t\tfoo\n", "    foo\n\tbar\n", " - foo\n   - bar\n\t - baz\n", "#\tFoo\n", "*\t*\t*\t\n",
    # precedence, thematic breaks
    "- `one\n- two`\n", "***\n---\n___\n", "+++\n", "===\n", "--\n**\n__\n", " ***\n  ***\n   ***\n", "    ***\n",
    "Foo\n    ***\n", "_____________________________________\n", " - - -\n", " **  * ** * ** * **\n", "-     -      -      -\n",
    "- - - -    \n", "_ _ _ _ a\n\na------\n\n---a---\n", " *-*\n", "- foo\n***\n- bar\n", "Foo\n***\nbar\n",
    "Foo\n---\nbar\n", "* Foo\n* * *\n* Bar\n", "- Foo\n- * * *\n",
    # ATX headings
    "# foo\n## foo\n### foo\n#### foo\n##### foo\n###### foo\n", "####### foo\n", "#5 bolt\n\n#hashtag\n",
    "\\## foo\n", "# foo *bar* \\*baz\\*\n", "#                  foo                     \n", " ### foo\n  ## foo\n   # foo\n",
    "    # foo\n", "foo\n    # bar\n", "## foo ##\n  ###   bar    ###\n", "# foo ##################################\n##### foo ##\n",
    "### foo ###     \n", "### foo ### b\n", "# foo#\n", "### foo \\###\n## foo #\\##\n# foo \\#\n", "****\n## foo\n****\n",
    "Foo bar\n# baz\nBar foo\n", "## \n#\n### ###\n",
    # setext headings
    "Foo *bar*\n=========\n\nFoo *bar*\n---------\n", "Foo *bar\nbaz*\n====\n", "  Foo *bar\nbaz*\t\n====\n",
    "Foo\n-------------------------\n\nFoo\n=\n", "   Foo\n---\n\n  Foo\n-----\n\n  Foo\n  ===\n", "    Foo\n    ---\n\n    Foo\n---\n",
    "Foo\n   ----      \n", "Foo\n    ---\n", "Foo\n= =\n\nFoo\n--- -\n", "Foo  \n-----\n", "Foo\\\n----\n",
    "`Foo\n----\n`\n\n<a title=\"a lot\n---\nof dashes\"/>\n", "> Foo\n---\n", "> foo\nbar\n===\n", "- Foo\n---\n",
    "Foo\nBar\n---\n", "---\nFoo\n---\nBar\n---\nBaz\n", "\n====\n", "---\n---\n", "- foo\n-----\n", "    foo\n---\n",
    "> foo\n-----\n", "\\> foo\n------\n", "Foo\n\nbar\n---\nbaz\n", "Foo\nbar\n\n---\n\nbaz\n", "Foo\nbar\n* * *\nbaz\n",
    "Foo\nbar\n\\---\nbaz\n",
    # indented code
    "    a simple\n      indented code block\n", "  - foo\n\n    bar\n", "1.  foo\n\n    - bar\n", "    <a/>\n    *hi*\n\n    - one\n",
    "    chunk1\n\n    chunk2\n  \n \n \n    chunk3\n", "    chunk1\n      \n      chunk2\n", "Foo\n    bar\n", "    foo\nbar\n",
    "# Heading\n    foo\nHeading\n------\n    foo\n----\n", "        foo\n    bar\n", "\n    \n    foo\n    \n\n", "    foo  \n",
    # fenced code
    "```\n<\n >\n```\n", "~~~\n<\n >\n~~~\n", "``\nfoo\n``\n", "```\naaa\n~~~\n```\n", "~~~\naaa\n```\n~~~\n",
    "````\naaa\n```\n``````\n", "~~~~\naaa\n~~~\n~~~~\n", "```\n", "`````\n\n```\naaa\n", "> ```\n> aaa\n\nbbb\n",
    "```\n\n  \n```\n", "```\n```\n", " ```\n aaa\naaa\n```\n", "  ```\naaa\n  aaa\naaa\n  ```\n",
    "   ```\n   aaa\n    aaa\n  aaa\n   ```\n", "    ```\n    aaa\n    ```\n", "```\naaa\n  ```\n", "   ```\naaa\n  ```\n",
    "```\naaa\n    ```\n", "``` ```\naaa\n", "~~~~~~\naaa\n~~~ ~~\n", "foo\n```\nbar\n```\nbaz\n", "foo\n---\n~~~\nbar\n~~~\n# baz\n",
    "```ruby\ndef foo(x)\n  return 3\nend\n```\n", "~~~~    ruby startline=3 $%@#$\ndef foo(x)\n~~~~~~~\n", "````;\n````\n",
    "``` aa ```\nfoo\n", "~~~ aa ``` ~~~\nfoo\n~~~\n", "```\n``` aaa\n```\n", "```foo\\+bar&amp;\nx\n```\n",
    # HTML blocks
    "<table><tr><td>\n<pre>\n**Hello**,\n\n_world_.\n</pre>\n</td></tr></table>\n", "<table>\n  <tr>\n    <td>\n           hi\n    </td>\n  </tr>\n</table>\n\nokay.\n",
    " <div>\n  *hello*\n         <foo><a>\n", "</div>\n*foo*\n", "<DIV CLASS=\"foo\">\n\n*Markdown*\n\n</DIV>\n",
    "<div id=\"foo\"\n  class=\"bar\">\n</div>\n", "<div id=\"foo\" class=\"bar\n  baz\">\n</div>\n", "<div>\n*foo*\n\n*bar*\n",
    "<div id=\"foo\"\n*hi*\n", "<div class\nfoo\n", "<div *???-&&&-<---\n*foo*\n", "<div><a href=\"bar\">*foo*</a></div>\n",
    "<table><tr><td>\nfoo\n</td></tr></table>\n", "<div></div>\n``` c\nint x = 33;\n```\n", "<a href=\"foo\">\n*bar*\n</a>\n",
    "<Warning>\n*bar*\n</Warning>\n", "<i class=\"foo\">\n*bar*\n</i>\n", "</ins>\n*bar*\n", "<del>\n*foo*\n</del>\n",
    "<del>\n\n*foo*\n\n</del>\n", "<del>*foo*</del>\n", "<pre language=\"haskell\"><code>\nimport Text.HTML.TagSoup\n\nmain :: IO ()\n</code></pre>\nokay\n",
    "<script type=\"text/javascript\">\n// JavaScript example\n\ndocument.getElementById(\"demo\").innerHTML = \"Hello JavaScript!\";\n</script>\nokay\n",
    "<textarea>\n\n*foo*\n\n_bar_\n\n</textarea>\n", "<style\n  type=\"text/css\">\nh1 {color:red;}\n\np {color:blue;}\n</style>\nokay\n",
    "<style\n  type=\"text/css\">\n\nfoo\n", "> <div>\n> foo\n\nbar\n", "- <div>\n- foo\n", "<style>p{color:red;}</style>\n*foo*\n",
    "<!-- foo -->*bar*\n*baz*\n", "<script>\nfoo\n</script>1. *bar*\n", "<!-- Foo\n\nbar\n   baz -->\nokay\n",
    "<?php\n\n  echo '>';\n\n?>\nokay\n", "<!DOCTYPE html>\n", "<![CDATA[\nfunction matchwo(a,b)\n{\n  if (a < b && a < 0) then {\n    return 1;\n\n  } else {\n\n    return 0;\n  }\n}\n]]>\nokay\n",
    "  <!-- foo -->\n\n    <!-- foo -->\n", "  <div>\n\n    <div>\n", "Foo\n<div>\nbar\n</div>\n", "<div>\nbar\n</div>\n*foo*\n",
    "Foo\n<a href=\"bar\">\nbaz\n", "<div>\n\n*Emphasized* text.\n\n</div>\n", "<div>\n*Emphasized* text.\n</div>\n",
    "<table>\n\n<tr>\n\n<td>\nHi\n</td>\n\n</tr>\n\n</table>\n", "<table>\n\n  <tr>\n\n    <td>\n      Hi\n    </td>\n\n  </tr>\n\n</table>\n",
    "<search>\n*x*\n</search>\n", "<!-->\nfoo\n", "<!--->\nfoo\n",
    # link reference definitions
    "[foo]: /url \"title\"\n\n[foo]\n", "   [foo]: \n      /url  \n           'the title'  \n\n[foo]\n",
    "[Foo*bar\\]]:my_(url) 'title (with parens)'\n\n[Foo*bar\\]]\n", "[Foo bar]:\n<my url>\n'title'\n\n[Foo bar]\n",
    "[foo]: /url '\ntitle\nline1\nline2\n'\n\n[foo]\n", "[foo]: /url 'title\n\nwith blank line'\n\n[foo]\n", "[foo]:\n/url\n\n[foo]\n",
    "[foo]:\n\n[foo]\n", "[foo]: <>\n\n[foo]\n", "[foo]: <bar>(baz)\n\n[foo]\n", "[foo]: /url\\bar\\*baz \"foo\\\"bar\\baz\"\n\n[foo]\n",
    "[foo]\n\n[foo]: url\n", "[foo]\n\n[foo]: first\n[foo]: second\n", "[FOO]: /url\n\n[Foo]\n", "[ΑΓΩ]: /φου\n\n[αγω]\n",
    "[foo]: /url\n", "[\nfoo\n]: /url\nbar\n", "[foo]: /url \"title\" ok\n", "[foo]: /url\n\"title\" ok\n", "    [foo]: /url \"title\"\n\n[foo]\n",
    "```\n[foo]: /url\n```\n\n[foo]\n", "Foo\n[bar]: /baz\n\n[bar]\n", "# [Foo]\n[foo]: /url\n> bar\n", "[foo]: /url\nbar\n===\n[foo]\n",
    "[foo]: /url\n===\n[foo]\n", "[foo]: /foo-url \"foo\"\n[bar]: /bar-url\n  \"bar\"\n[baz]: /baz-url\n\n[foo],\n[bar],\n[baz]\n",
    "[foo]\n\n> [foo]: /url\n", "[ẞ]\n\n[SS]: /url\n",
    # paragraphs, blank lines
    "aaa\n\nbbb\n", "aaa\nbbb\n\nccc\nddd\n", "aaa\n\n\nbbb\n", "  aaa\n bbb\n", "aaa\n             bbb\n                                       ccc\n",
    "   aaa\nbbb\n", "    aaa\nbbb\n", "aaa     \nbbb     \n", "  \n\naaa\n  \n\n# aaa\n\n  \n",
    # block quotes
    "> # Foo\n> bar\n> baz\n", "># Foo\n>bar\n> baz\n", "   > # Foo\n   > bar\n > baz\n", "    > # Foo\n    > bar\n    > baz\n",
    "> # Foo\n> bar\nbaz\n", "> bar\nbaz\n> foo\n", "> foo\n---\n", "> - foo\n- bar\n", ">     foo\n    bar\n", "> ```\nfoo\n```\n",
    "> foo\n    - bar\n", ">\n", ">\n>  \n> \n", ">\n> foo\n>  \n", "> foo\n\n> bar\n", "> foo\n> bar\n", "> foo\n>\n> bar\n",
    "foo\n> bar\n", "> aaa\n***\n> bbb\n", "> bar\nbaz\n", "> bar\n\nbaz\n", "> bar\n>\nbaz\n", "> > > foo\nbar\n",
    ">>> foo\n> bar\n>>baz\n", ">     code\n\n>    not code\n",
    # list items
    "A paragraph\nwith two lines.\n\n    indented code\n\n> A block quote.\n", "1.  A paragraph\n    with two lines.\n\n        indented code\n\n    > A block quote.\n",
    "- one\n\n two\n", "- one\n\n  two\n", " -    one\n\n     two\n", " -    one\n\n      two\n", "   > > 1.  one\n>>\n>>     two\n",
    ">>- one\n>>\n  >  > two\n", "-one\n\n2.two\n", "- foo\n\n\n  bar\n", "1.  foo\n\n    ```\n    bar\n    ```\n\n    baz\n\n    > bam\n",
    "- Foo\n\n      bar\n\n\n      baz\n", "123456789. ok\n", "1234567890. not ok\n", "0. ok\n", "003. ok\n", "-1. not ok\n",
    "- foo\n\n      bar\n", "  10.  foo\n\n           bar\n", "    indented code\n\nparagraph\n\n    more code\n",
    "1.     indented code\n\n   paragraph\n\n       more code\n", "1.      indented code\n\n   paragraph\n\n       more code\n",
    "   foo\n\nbar\n", "-    foo\n\n  bar\n", "-  foo\n\n   bar\n", "-\n  foo\n-\n  ```\n  bar\n  ```\n-\n      baz\n", "-   \n  foo\n",
    "-\n\n  foo\n", "- foo\n-\n- bar\n", "- foo\n-   \n- bar\n", "1. foo\n2.\n3. bar\n", "*\n", "foo\n*\n\nfoo\n1.\n",
    " 1.  A paragraph\n     with two lines.\n", "    1.  A paragraph\n        with two lines.\n", "  1.  A paragraph\nwith two lines.\n",
    "> 1. > Blockquote\ncontinued here.\n", "- foo\n  - bar\n    - baz\n      - boo\n", "- foo\n - bar\n  - baz\n   - boo\n",
    "10) foo\n    - bar\n", "10) foo\n   - bar\n", "- - foo\n", "1. - 2. foo\n", "- # Foo\n- Bar\n  ---\n  baz\n",
    # lists
    "- foo\n- bar\n+ baz\n", "1. foo\n2. bar\n3) baz\n", "Foo\n- bar\n- baz\n", "The number of windows in my house is\n14.  The number of doors is 6.\n",
    "The number of windows in my house is\n1.  The number of doors is 6.\n", "- foo\n\n- bar\n\n\n- baz\n", "- foo\n  - bar\n    - baz\n\n\n      bim\n",
    "- foo\n- bar\n\n<!-- -->\n\n- baz\n- bim\n", "-   foo\n\n    notcode\n\n-   foo\n\n<!-- -->\n\n    code\n",
    "- a\n - b\n  - c\n   - d\n  - e\n - f\n- g\n", "1. a\n\n  2. b\n\n   3. c\n", "- a\n - b\n  - c\n   - d\n    - e\n",
    "1. a\n\n  2. b\n\n    3. c\n", "- a\n- b\n\n- c\n", "* a\n*\n\n* c\n", "- a\n- b\n\n  c\n- d\n", "- a\n- b\n\n  [ref]: /url\n- d\n",
    "- a\n- ```\n  b\n\n\n  ```\n- c\n", "- a\n  - b\n\n    c\n- d\n", "* a\n  > b\n  >\n* c\n", "- a\n  > b\n  ```\n  c\n  ```\n- d\n",
    "- a\n", "- a\n  - b\n", "1. ```\n   foo\n   ```\n\n   bar\n", "* foo\n  * bar\n\n  baz\n", "- a\n  - b\n  - c\n\n- d\n  - e\n  - f\n",
    # inlines: backslash escapes, entities
    "\\!\\\"\\#\\$\\%\\&\\'\\(\\)\\*\\+\\,\\-\\.\\/\\:\\;\\<\\=\\>\\?\\@\\[\\\\\\]\\^\\_\\`\\{\\|\\}\\~\n", "\\\t\\A\\a\\ \\3\\φ\\«\n",
    "\\*not emphasized*\n\\<br/> not a tag\n\\[not a link](/foo)\n\\`not code`\n1\\. not a list\n\\* not a list\n\\# not a heading\n\\[foo]: /url \"not a reference\"\n\\&ouml; not a character entity\n",
    "\\\\*emphasis*\n", "foo\\\nbar\n", "`` \\[\\` ``\n", "    \\[\\]\n", "~~~\n\\[\\]\n~~~\n", "<https://example.com?find=\\*>\n",
    "<a href=\"/bar\\/)\">\n", "[foo](/bar\\* \"ti\\*tle\")\n", "[foo]\n\n[foo]: /bar\\* \"ti\\*tle\"\n", "``` foo\\+bar\nfoo\n```\n",
    "&nbsp; &amp; &copy; &AElig; &Dcaron;\n&frac34; &HilbertSpace; &DifferentialD;\n&ClockwiseContourIntegral; &ngE;\n",
    "&#35; &#1234; &#992; &#0;\n", "&#X22; &#XD06; &#xcab;\n", "&nbsp &x; &#; &#x;\n&#87654321;\n&#abcdef0;\n&ThisIsNotDefined; &hi?;\n",
    "&copy\n", "&MadeUpEntity;\n", "<a href=\"&ouml;&ouml;.html\">\n", "[foo](/f&ouml;&ouml; \"f&ouml;&ouml;\")\n",
    "``` f&ouml;&ouml;\nfoo\n```\n", "`f&ouml;&ouml;`\n", "    f&ouml;f&ouml;\n", "&#42;foo&#42;\n*foo*\n", "&#42; foo\n\n* foo\n",
    "foo&#10;&#10;bar\n", "&#9;foo\n", "[a](url &quot;tit&quot;)\n",
    # code spans
    "`foo`\n", "`` foo ` bar ``\n", "` `` `\n", "`  ``  `\n", "` a`\n", "` b `\n", "` `\n`  `\n", "``\nfoo\nbar  \nbaz\n``\n",
    "``\nfoo \n``\n", "`foo   bar \nbaz`\n", "`foo\\`bar`\n", "``foo`bar``\n", "` foo `` bar `\n", "*foo`*`\n", "[not a `link](/foo`)\n",
    "`<a href=\"`\">`\n", "<a href=\"`\">`\n", "`<https://foo.bar.`baz>`\n", "<https://foo.bar.`baz>`\n", "```foo``\n", "`foo\n", "`foo``bar``\n",
    # emphasis
    "*foo bar*\n", "a * foo bar*\n", "a*\"foo\"*\n", "* a *\n", "*$*alpha.\n\n*£*bravo.\n\n*€*charlie.\n", "foo*bar*\n", "5*6*78\n",
    "_foo bar_\n", "_ foo bar_\n", "a_\"foo\"_\n", "foo_bar_\n", "5_6_78\n", "пристаням_стремятся_\n", "aa_\"bb\"_cc\n", "foo-_(bar)_\n",
    "_foo*\n", "*foo bar *\n", "*foo bar\n*\n", "*(*foo)\n", "*(*foo*)*\n", "*foo*bar\n", "_foo bar _\n", "_(_foo)\n", "_(_foo_)_\n",
    "_foo_bar\n", "_пристаням_стремятся\n", "_foo_bar_baz_\n", "_(bar)_.\n", "**foo bar**\n", "** foo bar**\n", "a**\"foo\"**\n",
    "foo**bar**\n", "__foo bar__\n", "__ foo bar__\n", "__\nfoo bar__\n", "a__\"foo\"__\n", "foo__bar__\n", "5__6__78\n",
    "пристаням__стремятся__\n", "__foo, __bar__, baz__\n", "foo-__(bar)__\n", "**foo bar **\n", "**(**foo)\n", "*(**foo**)*\n",
    "**Gomphocarpus (*Gomphocarpus physocarpus*, syn.\n*Asclepias physocarpa*)**\n", "**foo \"*bar*\" foo**\n", "**foo**bar\n",
    "__foo bar __\n", "__(__foo)\n", "_(__foo__)_\n", "__foo__bar\n", "__пристаням__стремятся\n", "__foo__bar__baz__\n", "__(bar)__.\n",
    "*foo [bar](/url)*\n", "*foo\nbar*\n", "_foo __bar__ baz_\n", "_foo _bar_ baz_\n", "__foo_ bar_\n", "*foo *bar**\n", "*foo **bar** baz*\n",
    "*foo**bar**baz*\n", "*foo**bar*\n", "***foo** bar*\n", "*foo **bar***\n", "*foo**bar***\n", "foo***bar***baz\n",
    "foo******bar*********baz\n", "*foo **bar *baz* bim** bop*\n", "*foo [*bar*](/url)*\n", "** is not an empty emphasis\n",
    "**** is not an empty strong emphasis\n", "**foo [bar](/url)**\n", "**foo\nbar**\n", "__foo _bar_ baz__\n", "__foo __bar__ baz__\n",
    "____foo__ bar__\n", "**foo **bar****\n", "**foo *bar* baz**\n", "**foo*bar*baz**\n", "***foo* bar**\n", "**foo *bar***\n",
    "**foo *bar **baz**\nbim* bop**\n", "**foo [*bar*](/url)**\n", "__ is not an empty emphasis\n", "____ is not an empty strong emphasis\n",
    "foo ***\n", "foo *\\**\n", "foo *_*\n", "foo *****\n", "foo **\\***\n", "foo **_**\n", "**foo*\n", "*foo**\n", "***foo**\n",
    "****foo*\n", "**foo***\n", "*foo****\n", "foo ___\n", "foo _\\__\n", "foo _*_\n", "foo _____\n", "foo __\\___\n", "foo __*__\n",
    "__foo_\n", "_foo__\n", "___foo__\n", "____foo_\n", "__foo___\n", "_foo____\n", "**foo**\n", "*_foo_*\n", "__foo__\n", "_*foo*_\n",
    "****foo****\n", "____foo____\n", "******foo******\n", "***foo***\n", "_____foo_____\n", "*foo _bar* baz_\n", "*foo __bar *baz bim__ bam*\n",
    "**foo **bar baz**\n", "*foo *bar baz*\n", "*[bar*](/url)\n", "_foo [bar_](/url)\n", "*<img src=\"foo\" title=\"*\"/>\n", "**<a href=\"**\">\n",
    "__<a href=\"__\">\n", "*a `*`*\n", "_a `_`_\n", "**a<https://foo.bar/?q=**>\n", "__a<https://foo.bar/?q=__>\n",
    # links
    "[link](/uri \"title\")\n", "[link](/uri)\n", "[](./target.md)\n", "[link]()\n", "[link](<>)\n", "[]()\n", "[link](/my uri)\n",
    "[link](</my uri>)\n", "[link](foo\nbar)\n", "[link](<foo\nbar>)\n", "[a](<b)c>)\n", "[link](<foo\\>)\n", "[a](<b)c\n[a](<b)c>\n[a](<b>c)\n",
    "[link](\\(foo\\))\n", "[link](foo(and(bar)))\n", "[link](foo(and(bar))\n", "[link](foo\\(and\\(bar\\))\n", "[link](<foo(and(bar)>)\n",
    "[link](foo\\)\\:)\n", "[link](#fragment)\n\n[link](https://example.com#fragment)\n\n[link](https://example.com?foo=3#frag)\n",
    "[link](foo\\bar)\n", "[link](foo%20b&auml;)\n", "[link](\"title\")\n", "[link](/url \"title\")\n[link](/url 'title')\n[link](/url (title))\n",
    "[link](/url \"title \\\"&quot;\")\n", "[link](/url\u00a0\"title\")\n", "[link](/url \"title \"and\" title\")\n", "[link](/url 'title \"and\" title')\n",
    "[link](   /uri\n  \"title\"  )\n", "[link] (/uri)\n", "[link [foo [bar]]](/uri)\n", "[link] bar](/uri)\n", "[link [bar](/uri)\n",
    "[link \\[bar](/uri)\n", "[link *foo **bar** `#`*](/uri)\n", "[![moon](moon.jpg)](/uri)\n", "[foo [bar](/uri)](/uri)\n",
    "[foo *[bar [baz](/uri)](/uri)*](/uri)\n", "![[[foo](uri1)](uri2)](uri3)\n", "*[foo*](/uri)\n", "[foo *bar](baz*)\n", "*foo [bar* baz]\n",
    "[foo <bar attr=\"](baz)\">\n", "[foo`](/uri)`\n", "[foo<https://example.com/?search=](uri)>\n", "[foo][bar]\n\n[bar]: /url \"title\"\n",
    "[link [foo [bar]]][ref]\n\n[ref]: /uri\n", "[link \\[bar][ref]\n\n[ref]: /uri\n", "[link *foo **bar** `#`*][ref]\n\n[ref]: /uri\n",
    "[![moon](moon.jpg)][ref]\n\n[ref]: /uri\n", "[foo [bar](/uri)][ref]\n\n[ref]: /uri\n", "[foo *bar [baz][ref]*][ref]\n\n[ref]: /uri\n",
    "*[foo*][ref]\n\n[ref]: /uri\n", "[foo *bar][ref]*\n\n[ref]: /uri\n", "[foo <bar attr=\"][ref]\">\n\n[ref]: /uri\n",
    "[foo`][ref]`\n\n[ref]: /uri\n", "[foo<https://example.com/?search=][ref]>\n\n[ref]: /uri\n", "[foo][BaR]\n\n[bar]: /url \"title\"\n",
    "[ẞ]\n\n[SS]: /url\n", "[Foo\n  bar]: /url\n\n[Baz][Foo bar]\n", "[foo] [bar]\n\n[bar]: /url \"title\"\n", "[foo]\n[bar]\n\n[bar]: /url \"title\"\n",
    "[foo]: /url1\n\n[foo]: /url2\n\n[bar][foo]\n", "[bar][foo\\!]\n\n[foo!]: /url\n", "[foo][ref[]\n\n[ref[]: /uri\n",
    "[foo][ref[bar]]\n\n[ref[bar]]: /uri\n", "[[[foo]]]\n\n[[[foo]]]: /url\n", "[foo][ref\\[]\n\n[ref\\[]: /uri\n", "[bar\\\\]: /uri\n\n[bar\\\\]\n",
    "[]\n\n[]: /uri\n", "[\n ]\n\n[\n ]: /uri\n", "[foo][]\n\n[foo]: /url \"title\"\n", "[*foo* bar][]\n\n[*foo* bar]: /url \"title\"\n",
    "[Foo][]\n\n[foo]: /url \"title\"\n", "[foo] \n[]\n\n[foo]: /url \"title\"\n", "[foo]\n\n[foo]: /url \"title\"\n",
    "[*foo* bar]\n\n[*foo* bar]: /url \"title\"\n", "[[*foo* bar]]\n\n[*foo* bar]: /url \"title\"\n", "[[bar [foo]\n\n[foo]: /url\n",
    "[Foo]\n\n[foo]: /url \"title\"\n", "[foo] bar\n\n[foo]: /url\n", "\\[foo]\n\n[foo]: /url \"title\"\n", "[foo*]: /url\n\n*[foo*]\n",
    "[foo][bar]\n\n[foo]: /url1\n[bar]: /url2\n", "[foo][]\n\n[foo]: /url1\n", "[foo]()\n\n[foo]: /url1\n", "[foo](not a link)\n\n[foo]: /url1\n",
    "[foo][bar][baz]\n\n[baz]: /url\n", "[foo][bar][baz]\n\n[baz]: /url1\n[bar]: /url2\n", "[foo][bar][baz]\n\n[baz]: /url1\n[foo]: /url2\n",
    # images
    "![foo](/url \"title\")\n", "![foo *bar*]\n\n[foo *bar*]: train.jpg \"train & tracks\"\n", "![foo ![bar](/url)](/url2)\n",
    "![foo [bar](/url)](/url2)\n", "![foo *bar*][]\n\n[foo *bar*]: train.jpg \"train & tracks\"\n", "![foo *bar*][foobar]\n\n[FOOBAR]: train.jpg \"train & tracks\"\n",
    "![foo](train.jpg)\n", "My ![foo bar](/path/to/train.jpg  \"title\"   )\n", "![foo](<url>)\n", "![](/url)\n", "![foo][bar]\n\n[bar]: /url\n",
    "![foo][bar]\n\n[BAR]: /url\n", "![foo][]\n\n[foo]: /url \"title\"\n", "![*foo* bar][]\n\n[*foo* bar]: /url \"title\"\n", "![Foo][]\n\n[foo]: /url \"title\"\n",
    "![foo] \n[]\n\n[foo]: /url \"title\"\n", "![foo]\n\n[foo]: /url \"title\"\n", "![*foo* bar]\n\n[*foo* bar]: /url \"title\"\n",
    "![[foo]]\n\n[[foo]]: /url \"title\"\n", "![Foo]\n\n[foo]: /url \"title\"\n", "!\\[foo]\n\n[foo]: /url \"title\"\n", "\\![foo]\n\n[foo]: /url \"title\"\n",
    "![a\nb `c` *d*](/u)\n",
    # autolinks, raw HTML
    "<http://foo.bar.baz>\n", "<https://foo.bar.baz/test?q=hello&id=22&boolean>\n", "<irc://foo.bar:2233/baz>\n", "<MAILTO:FOO@BAR.BAZ>\n",
    "<a+b+c:d>\n", "<made-up-scheme://foo,bar>\n", "<https://../>\n", "<localhost:5001/foo>\n", "<https://foo.bar/baz bim>\n",
    "<https://example.com/\\[\\>\n", "<foo@bar.example.com>\n", "<foo+special@Bar.baz-bar0.com>\n", "<foo\\+@bar.example.com>\n", "<>\n",
    "< https://foo.bar >\n", "<m:abc>\n", "<foo.bar.baz>\n", "https://example.com\n", "foo@bar.example.com\n", "<a><bab><c2c>\n", "<a/><b2/>\n",
    "<a  /><b2\ndata=\"foo\" >\n", "<a foo=\"bar\" bam = 'baz <em>\"</em>'\n_boolean zoop:33=zoop:33 />\n", "Foo <responsive-image src=\"foo.jpg\" />\n",
    "<33> <__>\n", "<a h*#ref=\"hi\">\n", "<a href=\"hi'> <a href=hi'>\n", "< a><\nfoo><bar/ >\n<foo bar=baz\nbim!bop />\n", "<a href='bar'title=title>\n",
    "</a></foo >\n", "</a href=\"foo\">\n", "foo <!-- this is a --\ncomment - with hyphens -->\n", "foo <!--> foo -->\n\nfoo <!---> foo -->\n",
    "foo <?php echo $a; ?>\n", "foo <!ELEMENT br EMPTY>\n", "foo <![CDATA[>&<]]>\n", "foo <a href=\"&ouml;\">\n", "foo <a href=\"\\*\">\n",
    "<a href=\"\\\"\">\n",
    # breaks, text
    "foo  \nbaz\n", "foo\\\nbaz\n", "foo       \nbaz\n", "foo  \n     bar\n", "foo\\\n     bar\n", "*foo  \nbar*\n", "*foo\\\nbar*\n",
    "`code  \nspan`\n", "`code\\\nspan`\n", "<a href=\"foo  \nbar\">\n", "<a href=\"foo\\\nbar\">\n", "foo\\\n", "foo  \n", "### foo\\\n",
    "### foo  \n", "foo\nbaz\n", "foo \n baz\n", "hello $.;'there\n", "Foo χρῆν\n", "Multiple     spaces\n",
    # GFM
    "| foo | bar |\n| --- | --- |\n| baz | bim |\n", "| abc | defghi |\n:-: | -----------:\nbar | baz\n", "| f\\|oo  |\n| ------ |\n| b `\\|` az |\n| b **\\|** im |\n",
    "| abc | def |\n| --- | --- |\n| bar | baz |\n> bar\n", "| abc | def |\n| --- | --- |\n| bar | baz |\nbar\n\nbar\n", "| abc | def |\n| --- |\n| bar |\n",
    "| abc | def |\n| --- | --- |\n| bar |\n| bar | baz | boo |\n", "| abc | def |\n| --- | --- |\n", "- [ ] foo\n- [x] bar\n",
    "- [x] foo\n  - [ ] bar\n  - [x] baz\n- [ ] bim\n", "~~Hi~~ Hello, ~there~ world!\n", "This ~~has a\n\nnew paragraph~~.\n",
    "This will ~~~not~~~ strike.\n", "www.commonmark.org\n", "Visit www.commonmark.org/help for more information.\n",
    "Visit www.commonmark.org.\n\nVisit www.commonmark.org/a.b.\n", "www.google.com/search?q=Markup+(business)\n\nwww.google.com/search?q=Markup+(business)))\n\n(www.google.com/search?q=Markup+(business))\n\n(www.google.com/search?q=Markup+(business)\n",
    "www.google.com/search?q=(business))+ok\n", "www.google.com/search?q=commonmark&hl=en\n\nwww.google.com/search?q=commonmark&hl;\n",
    "www.commonmark.org/he<lp\n", "http://commonmark.org\n\n(Visit https://encrypted.google.com/search?q=Markup+(business))\n",
    "foo@bar.baz\n", "hello@mail+xyz.example isn't valid, but hello+xyz@mail.example is.\n", "a.b-c_d@a.b\n\na.b-c_d@a.b.\n\na.b-c_d@a.b-\n\na.b-c_d@a.b_\n",
    "<strong> <title> <style> <em>\n\n<blockquote>\n  <xmp> is disallowed.  <XMP> is also disallowed.\n</blockquote>\n",
    "|a|\n|-|\n|b|\n- c\n", "a|b\n-|-\nc\n\nd\n", "| a |\n| :-: |\n| `x|y` |\n", "- [ ] \n- [x]\n", "- [ ]a\n", "1. [x] done\n",
    "> - [ ] quoted task\n", "* [X] upper\n",
]


def random_inline():
    pieces = ["foo", "bar", " ", "  ", "*", "**", "_", "__", "~", "~~", "`", "``", "[", "]", "(", ")", "![", "<", ">", "&amp;", "&",
              "\\", "\\*", "<http://a.b>", "<a@b.c>", "www.x.org", "https://a.b/c", "x@y.zz", "<em>", "</em>", "<!-- c -->", "\"",
              "'", "!", "#", "|", "ą", "日本", " (", ") ", "[x]", "[x][]", "[x](/u)", "[y](</a b> \"t\")", "\n", "  \n", "\\\n"]
    return "".join(R.choice(pieces) for _ in range(R.randint(1, 12)))


def random_document():
    prefixes = ["", "", "", "> ", "- ", "* ", "1. ", "2) ", "  ", "    ", "\t", "   - ", "> > ", "- > ", "1. - "]
    starts = ["", "", "", "# ", "## ", "###### ", "```\n", "~~~ py\n", "---", "***", "===", "<div>", "<!-- ", "[x]: /url\n",
              "| a | b |\n| - | :-: |\n", "- [ ] ", "- [x] "]
    lines = []
    for _ in range(R.randint(1, 10)):
        line = R.choice(prefixes) + R.choice(starts) + (random_inline() if R.random() < 0.8 else "")
        lines.append(line)
        if R.random() < 0.25:
            lines.append("")
    return "\n".join(lines) + ("\n" if R.random() < 0.9 else "")


cases = [("c", c) for c in CORPUS] + [("g", c) for c in CORPUS]
for _ in range(N):
    cases.append((R.choice("cg"), random_document()))


def ask(prog, cases):
    q = "".join("%s %s\n" % (m, t.encode().hex()) for m, t in cases)
    out = subprocess.run([prog], input=q.encode(), capture_output=True, check=True).stdout.decode().split("\n")
    return [bytes.fromhex(x).decode("utf-8", "replace") for x in out[:len(cases)]]


theirs = ask(ORACLE, cases)
ours = ask(DRIVER, cases)
agree, differ = [], 0
corpus_differ = 0
spacing = 0
for k, ((mode, text), a, b) in enumerate(zip(cases, theirs, ours)):
    if normalize(a) == normalize(b):
        agree.append((mode, text, b))
    elif re.sub(r"\s+", "", normalize(a)) == re.sub(r"\s+", "", normalize(b)):
        # white space only: md4c expands tabs in raw HTML, and an unclosed
        # comment keeps the line breaks next to tags as its text
        spacing += 1
        agree.append((mode, text, b))
    else:
        differ += 1
        if k < 2 * len(CORPUS):
            corpus_differ += 1
        if differ <= SHOW and (os.environ.get("ONLY") in (None, mode)):
            sys.stderr.write("differs (%s): %r\n--- md4c\n%s--- ours\n%s\n" % (mode, text, a, b))


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
print("// Generated by tools/markdown_vectors.py: do not edit. Documents whose HTML")
print("// agrees with md4c's after CommonMark's normalization; the HTML is the library's")
print("// (raw HTML kept, URLs unfiltered; github: GitHub's extensions on).")
print("#pragma once")
print()
print("struct MarkdownVector {")
print("    bool github;")
print("    const char* text;")
print("    const char* html;")
print("};")
print()
print("inline const MarkdownVector MarkdownVectors[] = {")
for mode, text, h in agree:
    print("    {%s, %s, %s}," % ("true" if mode == "g" else "false", c_literal(text), c_literal(h)))
print("};")
sys.stderr.write("%d agree (%d in white space only), %d differ (corpus %d of %d)\n"
                 % (len(agree), spacing, differ, corpus_differ, 2 * len(CORPUS)))
