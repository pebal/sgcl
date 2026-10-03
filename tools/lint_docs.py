#!/usr/bin/env python3
# SGCL: a C++20 application platform
# Copyright (c) 2022-2026 Sebastian Nibisz
# SPDX-License-Identifier: Apache-2.0
"""The form of the pages of docs/sgcl, checked against docs/STYLE.md, section 1.

What tools/run_blocks.py does for the programs, this does for the rest of a
page: it reads the Markdown and reports what breaks a rule of the form, one
line per finding, `path:line: rule: what`. It compiles nothing.

  tools/lint_docs.py                     every page under docs/sgcl
  tools/lint_docs.py PAGE|DIR ...        those pages, or the pages under DIR
  tools/lint_docs.py --changed           the pages that differ from git HEAD

The exit status is 1 when anything was found. The rules, by name:

  breadcrumb      the first line is `[sgcl](…) › …` (not on docs/sgcl/README.md)
  title           one `# ` title, `sgcl::`-qualified (a benchmarks page and
                  the list of the modules excepted); `<` and `>` escaped; a
                  class template's title carries its parameters
  html            no HTML tag outside code
  link-text       no backticks and no unescaped `<` in the text of a link
  link            a relative link names a file that exists, and its anchor
                  a heading of that file
  table           every table has a header row, each cell a capital letter
                  or a code span first, the first column not named Page
  fragment        a ```cpp block after the first is a program (`int main(`)
                  or declarations alone (a deduction guide, a specialization):
                  no statement, call or assignment outside a program
  detail          a program names nothing from a detail namespace: what an
                  example needs and the public API does not give is a gap in
                  the API, reported and filled
  overloads       overloads numbered `/*(n)*/` at the start of the line,
                  never `// (n)` after it
  sections        the sections a page of a method, of a class and of a
                  README has, in their order; `## Members` is the old form
  readme          a README has no program; its tables are alphabetical
  file-name       a file named with letters, digits, `_` and `-` alone; an
                  operator's page by cppreference's names (operator_at.md)
  async-pair      `async_x` is on the page of `x`, never a page of its own
  enum            an enumeration's page has the table `Value | Description`
  constant        a constant or a variable has no page of its own
"""

import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DOCS = os.path.join(ROOT, 'docs', 'sgcl')

CLASS_SECTIONS = ['Rules', 'Template parameters', 'Member types', 'Member objects', 'Member functions',
                  'Requirements', 'Non-member functions', 'Deduction guides', 'Specializations', 'Complexity',
                  'Iterator invalidation', 'Example', 'Examples', 'See also']
METHOD_SECTIONS = ['Parameters', 'Return value', 'Complexity', 'Exceptions', 'Notes', 'Example', 'See also']
METHOD_REQUIRED = ['Parameters', 'Return value', 'Complexity', 'Exceptions', 'Example', 'See also']
REQ_SECTIONS = ['Satisfied by', 'Notes', 'Example', 'See also']
OPERATOR_PAGES = {'operator_assign', 'operator_at', 'operator_cmp', 'operator_call', 'operator_deref',
                  'operator_bool', 'operator_arith', 'operator_inc', 'operator_conv'}
FUNCTION_SECTIONS = METHOD_SECTIONS
ENUM_SECTIONS = ['Rules', 'Example', 'Examples', 'See also']
README_SECTIONS = ['The rules', 'Pointers', 'Functions', 'Classes', 'Sequences and associative containers',
                   'Weak containers', 'Immutable containers', 'Mixins', 'Requirements', 'See also']


def slug(heading):
    """GitHub's anchor of a heading: lower case, punctuation dropped, spaces to hyphens."""
    text = heading.replace('\\', '').strip().lower()
    text = re.sub(r'[^\w\- ]', '', text)
    return text.replace(' ', '-')


_anchor_cache = {}


def anchors(path):
    if path not in _anchor_cache:
        found, code = set(), False
        try:
            with open(path, encoding='utf-8') as f:
                for line in f:
                    if line.startswith('```'):
                        code = not code
                    elif not code:
                        m = re.match(r'#{1,6} (.*)$', line.rstrip())
                        if m:
                            found.add(slug(m.group(1)))
        except OSError:
            pass
        _anchor_cache[path] = found
    return _anchor_cache[path]


def strip_code_spans(line):
    return re.sub(r'`[^`]*`', '``', line)


def kind_of(path):
    """method, class, req, readme, modules, benchmarks or other."""
    rel = os.path.relpath(path, DOCS)
    name = os.path.basename(path)
    directory = os.path.dirname(path)
    if rel == 'README.md':
        return 'modules'
    if name == 'README.md':
        return 'readme'
    if name == 'benchmarks.md':
        return 'benchmarks'
    if os.path.basename(directory) == 'req':
        return 'req'
    if os.path.exists(directory + '.md'):   # vector/insert.md next to vector.md
        return 'method'
    return 'class'


class Page:
    def __init__(self, path):
        self.path = path
        self.rel = os.path.relpath(path, ROOT)
        with open(path, encoding='utf-8') as f:
            self.lines = f.read().split('\n')
        self.kind = kind_of(path)
        self.findings = []
        self.is_constructor = False
        # the lines outside ``` blocks, and the blocks themselves
        self.prose, self.blocks, code = [], [], None
        for i, line in enumerate(self.lines, 1):
            if line.startswith('```'):
                if code is None:
                    code = (i, line[3:].strip(), [])
                else:
                    self.blocks.append(code)
                    code = None
            elif code is not None:
                code[2].append(line)
            else:
                self.prose.append((i, line))
        if self.kind == 'class':
            self.kind = self._kind_by_names()

    def _kind_by_names(self):
        """A page outside a class's directory: a class, a function, an enumeration or a constant, by what its
        names block declares under the name of its title."""
        if not self.blocks or self.blocks[0][1] != 'cpp':
            return 'class'
        titles = self.headings(1)
        if not titles:
            return 'class'
        name = re.split(r'::', titles[0][1].split(',')[0].split(' ')[0].replace('\\', ''))[-1]
        name = re.sub(r'<.*', '', name)
        text = '\n'.join(self.blocks[0][2])
        n = re.escape(name)
        if re.search(r'\benum\s+(class\s+|struct\s+)?' + n + r'\b', text):
            return 'enum'
        if re.search(r'\b(class|struct)\s+' + n + r'\b', text) or re.search(r'\bnamespace\s+[\w:]*\b' + n + r'\s*\{', text):
            return 'class'
        if re.search(r'\b' + n + r'\s*\(', text):
            return 'function'
        if re.search(r'\b(constexpr|const|extern)\b[^;(]*\b' + n + r'\b[^;(]*[;=]', text):
            return 'constant'
        return 'class'

    def report(self, line, rule, what):
        self.findings.append(f'{self.rel}:{line}: {rule}: {what}')

    def headings(self, level):
        mark = '#' * level + ' '
        return [(i, l[len(mark):].strip()) for i, l in self.prose if l.startswith(mark)]


def check_breadcrumb(p):
    if p.kind == 'modules':
        return
    first = next(((i, l) for i, l in enumerate(p.lines, 1) if l.strip()), (1, ''))
    if not re.match(r'\[sgcl\]\([^)]*\) › ', first[1]):
        p.report(first[0], 'breadcrumb', 'the page does not open with `[sgcl](…) › …`')


def check_title(p):
    titles = p.headings(1)
    if len(titles) != 1:
        p.report(titles[1][0] if len(titles) > 1 else 1, 'title', f'{len(titles)} titles, one expected')
        return
    line, title = titles[0]
    if p.kind not in ('modules', 'benchmarks') and not title.startswith('sgcl::'):
        p.report(line, 'title', 'not qualified with sgcl::')
    if re.search(r'(?<!\\)[<>]', strip_code_spans(title)):
        p.report(line, 'title', 'an unescaped < or >: write \\< and \\>')
    if p.kind == 'class' and p.blocks and '\\<' not in title:
        names = '\n'.join(p.blocks[0][2])
        last = re.split(r'::|, ', title.split(' ')[0])[-1]
        if re.search(r'template<[^\n]*>\s*\n?\s*(class|struct)\s+' + re.escape(last) + r'\b', names):
            p.report(line, 'title', f'{last} is a template: the title carries its parameters')


def check_html(p):
    for i, line in p.prose:
        if re.search(r'(?<!\\)<[A-Za-z/][^>\n]*(?<!\\)>', strip_code_spans(line)):
            p.report(i, 'html', 'an HTML tag outside code (or an unescaped <T>)')


LINK = re.compile(r'\[((?:[^\[\]]|\[[^\]]*\])*)\]\(([^)\s]+)\)')


def _links(p):
    """The links of the prose with the line they start on, a link whose text runs over a line break included."""
    prose = p.prose
    for k, (i, line) in enumerate(prose):
        for m in LINK.finditer(line):
            yield i, m
        if k + 1 < len(prose) and prose[k + 1][0] == i + 1 and line.strip():
            joined = line + ' ' + prose[k + 1][1]
            for m in LINK.finditer(joined):
                if m.start() < len(line) < m.end() - 1 and '](' not in line[m.start():]:
                    yield i, m


def check_links(p):
    for i, m in _links(p):
        text, target = m.group(1), m.group(2)
        if '`' in text:
            p.report(i, 'link-text', f'backticks in the text of [{text}]')
        if re.search(r'(?<!\\)<', text):
            p.report(i, 'link-text', f'an unescaped < in the text of [{text}]')
        if re.match(r'[a-z]+:', target):
            continue
        file, _, anchor = target.partition('#')
        dest = os.path.normpath(os.path.join(os.path.dirname(p.path), file)) if file else p.path
        if not os.path.exists(dest):
            p.report(i, 'link', f'{target}: no such file')
        elif anchor and dest.endswith('.md') and anchor not in anchors(dest):
            p.report(i, 'link', f'{target}: no such heading')


def check_tables(p):
    previous = ''
    for i, line in p.prose:
        if re.match(r'\|\s*:?-{3,}', line):
            header = previous
            cells = [c.strip() for c in header.strip().strip('|').split('|')]
            if not header.startswith('|') or not any(cells):
                p.report(i - 1, 'table', 'a table without a header row')
            else:
                if cells[0] == 'Page':
                    p.report(i - 1, 'table', 'the first column is named after what the table lists, not Page')
                for c in cells:
                    if not c or not (c[0].isupper() or c[0] in '`['):
                        p.report(i - 1, 'table', f'the column "{c}" does not start with a capital letter')
        previous = line


def is_statements(text):
    """Code that runs, not declarations: a call to print or assert, a member call, an assignment."""
    for line in text.split('\n'):
        l = line.split('//')[0].strip()
        if not l or l.startswith(('template', 'using', '#')):
            continue
        if re.search(r'\b(assert|static_assert|println|print)\(|std::cout|\w\.\w+\(|\w->\w', l):
            return True
        if l.endswith(';') and re.search(r'[^=!<>]=[^=]', l) and not re.search(r'= *(default|delete|0)\b', l) \
                and 'constexpr' not in l:
            return True
    return False


def check_blocks(p):
    for n, (line, lang, body) in enumerate(p.blocks):
        if lang != 'cpp':
            continue
        text = '\n'.join(body)
        program = 'int main(' in text
        if program:
            for k, l in enumerate(body, line + 1):
                if re.search(r'\bdetail::', l.split('//')[0]):
                    p.report(k, 'detail', 'a program names detail::, a gap in the public API to report')
        if p.kind in ('readme', 'modules') and program:
            p.report(line, 'readme', 'a README has no program')
        if n > 0 and not program and p.kind != 'readme' and is_statements(text):
            p.report(line, 'fragment', 'a cpp block that is not a program')
        if n == 0 and not program:
            for k, l in enumerate(body, line + 1):
                if re.search(r'//\s*\(\d+\)\s*$', l):
                    p.report(k, 'overloads', 'number an overload /*(n)*/ at the start of the line')


def check_order(p, allowed, required=()):
    seen = p.headings(2)
    names = [h for _, h in seen]
    for i, h in seen:
        if h == 'Members':
            p.report(i, 'sections', '`## Members` is the old form: tables, and a page per method')
        elif h not in allowed:
            p.report(i, 'sections', f'`## {h}` is not a section of this kind of page')
    order = [allowed.index(h) for h in names if h in allowed]
    if order != sorted(order):
        p.report(seen[0][0] if seen else 1, 'sections', 'the sections are out of order: ' + ', '.join(names))
    for r in required:
        if r not in names and not (r == 'Return value' and p.is_constructor):
            p.report(1, 'sections', f'`## {r}` is missing')


def check_sections(p):
    p.is_constructor = p.kind == 'method' and \
        os.path.basename(p.path) == os.path.basename(os.path.dirname(p.path)) + '.md'
    if p.kind == 'method':
        check_order(p, METHOD_SECTIONS, METHOD_REQUIRED)
    elif p.kind == 'function':
        check_order(p, FUNCTION_SECTIONS, METHOD_REQUIRED)
    elif p.kind == 'enum':
        check_order(p, ENUM_SECTIONS, ['See also'])
    elif p.kind == 'class':
        check_order(p, CLASS_SECTIONS)
    elif p.kind == 'req':
        check_order(p, REQ_SECTIONS, ['Satisfied by', 'Example', 'See also'])
    elif p.kind == 'readme':
        names = [h for _, h in p.headings(2)]
        if 'Example' in names or 'Examples' in names:
            p.report(1, 'readme', 'a README has no `## Example`')


def check_readme_tables(p):
    if p.kind not in ('readme', 'modules'):
        return
    rows = []
    for i, line in p.prose + [(0, '')]:
        m = re.match(r'\| \[([^\]]*)\]', line)
        if m:
            rows.append((i, m.group(1).replace('\\', '').lower()))
            continue
        names = [n for _, n in rows]
        if names != sorted(names):
            p.report(rows[0][0], 'readme', 'the rows of the table are not alphabetical')
        rows = []


def check_files(p):
    stem = os.path.basename(p.path)[:-3]
    if not re.fullmatch(r'[A-Za-z0-9_-]+', stem):
        p.report(1, 'file-name', f'{stem}.md: letters, digits, _ and - alone')
    if stem.startswith('operator') and stem not in OPERATOR_PAGES:
        p.report(1, 'file-name', f'{stem}.md: an operator page is one of ' + ', '.join(sorted(OPERATOR_PAGES)))
    if stem.startswith('async_') and os.path.exists(os.path.join(os.path.dirname(p.path), stem[6:] + '.md')):
        p.report(1, 'async-pair', f'{stem} belongs on the page of {stem[6:]}')
    if p.kind == 'constant' and stem != 'config':
        p.report(1, 'constant', 'a constant or a variable has no page: a row of the table that declares it')


def check_enum(p):
    if p.kind != 'enum':
        return
    if not any(re.match(r'\|\s*Value\s*\|', l) for _, l in p.prose):
        p.report(1, 'enum', 'no table `Value | Description` of the enumerators')


CHECKS = [check_files, check_enum, check_breadcrumb, check_title, check_html, check_links, check_tables, check_blocks, check_sections,
          check_readme_tables]


def pages(args):
    if not args:
        args = [DOCS]
    for a in args:
        a = os.path.abspath(a)
        if os.path.isdir(a):
            for d, _, files in os.walk(a):
                for f in sorted(files):
                    if f.endswith('.md'):
                        yield os.path.join(d, f)
        elif a.endswith('.md'):
            yield a


def changed():
    out = subprocess.run(['git', 'status', '--porcelain', '--untracked-files=all', '--', 'docs/sgcl'],
                         cwd=ROOT, capture_output=True, text=True, check=True).stdout
    return [os.path.join(ROOT, l[3:]) for l in out.splitlines() if l[3:].endswith('.md') and l[:2] != ' D']


def main(argv):
    args = argv[1:]
    targets = changed() if args == ['--changed'] else list(pages(args))
    findings = []
    for path in sorted(set(targets)):
        p = Page(path)
        for check in CHECKS:
            check(p)
        findings += p.findings
    for f in findings:
        print(f)
    print(f'{len(targets)} pages, {len(findings)} findings')
    return 1 if findings else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
