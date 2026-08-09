#!/usr/bin/env python3
"""One-time migration: rewrite Doxygen markup in C++ doc comments as reStructuredText.

Now that ``resources/mkdocs.py`` extracts comments verbatim, the headers
themselves have to hold Sphinx-ready text. This rewrites the comment bodies in
place, preserving the surrounding comment style and indentation:

    \\brief Foo              ->  Foo
    \\param x Description    ->  Args:\\n    x: Description
    \\return Description     ->  Returns:\\n    Description
    \\c x / <tt>x</tt>       ->  ``x``
    \\f$x\\f$                 ->  :math:`x`
    \\code ... \\endcode      ->  .. code-block:: c++

Usage:  doxygen_to_rst.py [--dry-run] <file-or-directory> ...
"""

import argparse
import os
import re
import sys
import textwrap

CPP_GROUP = r'([\w:]+)'
PARAM_GROUP = r'([\[\w:,\]]+)'

# Section tags that become Google-style docstring sections. Order matters only
# for the output; 'Args' and 'Template Args' are assembled from repeated tags.
SECTION_TAGS = [
    ('returns?', 'Returns'),
    ('throws?', 'Raises'),
    ('remarks?', 'Note'),
    ('note', 'Note'),
    ('warning', 'Warning'),
    ('sa|see', 'See Also'),
    ('deprecated', 'Deprecated'),
]
SECTION_ORDER = ['Args', 'Template Args', 'Returns', 'Raises',
                 'Warning', 'Note', 'See Also', 'Deprecated']

MARK = '\x00'
DOXYGEN_RE = re.compile(r'[\\@](brief|short|details|param|tparam|return|returns|'
                        r'throw|throws|remark|remarks|note|warning|sa|see|c|a|e|'
                        r'em|b|f\$|code|endcode|ingroup|ref|todo|deprecated|since)\b'
                        r'|<(tt|em|b|pre|ul|ol|li)>'
                        r'|(?:^|\s)%[A-Z]\w')

# Doxygen suppresses auto-linking of a word with a leading '%'. Only strip it
# from capitalized multi-character identifiers: '%u', '%i', '%f', '%s', '%lld'
# are printf specifiers appearing in the same comments and must survive.
NO_LINK_RE = re.compile(r'(^|\s)%([A-Z]\w+)')


CLOSERS = {')': '(', ']': '[', '}': '{'}


def split_word(token):
    """Split a token into the word itself and the punctuation that follows it.

    Doxygen applies an inline command to "the next word", which is not always
    an identifier: '\\c @', '\\c (uint32_t)-1' and '\\c -1' all occur. Trailing
    punctuation belongs to the sentence rather than to the word, except for a
    bracket closing one that the word itself opened: '\\c right]' is the word
    'right' inside an interval, whereas '\\c obs[i]' is a single subscript.
    """
    i = len(token)
    while i and token[i - 1] in ',.;:)]}':
        c = token[i - 1]
        if c in CLOSERS and token.count(CLOSERS[c], 0, i) >= token.count(c, 0, i):
            break
        i -= 1
    return token[:i], token[i:]


def inline_substitutions(s):
    for pattern, fmt in (('c', '``%s``'), ('a|e|em', '*%s*'), ('b', '**%s**')):
        def repl(m, fmt=fmt):
            word, tail = split_word(m.group(1))
            return (fmt % word if word else '') + tail
        s = re.sub(r'[\\@](?:%s)\s+(\S+)' % pattern, repl, s)
    # '</?tt>' rather than '</tt>': a handful of comments never close the tag
    s = re.sub(r'<tt>(.*?)</?tt>', r'``\1``', s, flags=re.DOTALL)
    s = re.sub(r'<em>(.*?)</em>', r'*\1*', s, flags=re.DOTALL)
    s = re.sub(r'<b>(.*?)</b>', r'**\1**', s, flags=re.DOTALL)
    s = re.sub(r'[\\@]f\$(.*?)[\\@]f\$', r':math:`\1`', s, flags=re.DOTALL)
    s = NO_LINK_RE.sub(r'\1\2', s)
    # '\\ref X' was an explicit cross-reference, so it becomes one. Sphinx
    # resolves an unqualified target against the enclosing class and module
    # before the global scope, so a bare sibling method name lands on the right
    # object. A target with no Python counterpart renders as a plain literal
    # and is reported by a nitpicky build, which is where the fixes come from.
    s = re.sub(r'[\\@]ref\s+(\w+)::(\w+)(\(\))?', r'`\1.\2\3`', s)
    s = re.sub(r'[\\@]ref\s+(\w+(?:\(\))?)', r'`\1`', s)
    s = re.sub(r'[\\@](brief|short|ref)\s*', '', s)
    s = re.sub(r'[\\@]ingroup\s+%s\s*' % CPP_GROUP, '', s)
    # '\file' documents the translation unit, not an entity
    s = re.sub(r'[\\@]file\s+\S*\s*', '', s)
    s = re.sub(r'[\\@]details\s*', '', s)
    s = s.replace('``true``', '``True``').replace('``false``', '``False``')
    return s


def block_substitutions(s):
    """Convert the block-level constructs that carry their own indentation."""
    # A member-group marker, optionally carrying the group title after '\name'
    s = re.sub(r'^[ \t]*[@\\][{}][ \t]*(?:[\\@]name[^\n]*)?$\n?', '', s, flags=re.M)
    # A rule of '=' or '-' used as a visual separator, which reStructuredText
    # would otherwise read as a section underline
    s = re.sub(r'^[ \t]*[=-]{6,}[ \t]*$\n?', '', s, flags=re.M)
    def code_block(m):
        body = textwrap.dedent(m.group(1).strip('\n'))
        return '\n.. code-block:: c++\n\n%s\n' % textwrap.indent(body, '    ')

    s = re.sub(r'[\\@]code(?:\{[^}]*\})?\n?(.*?)[\\@]endcode',
               code_block, s, flags=re.DOTALL)
    s = re.sub(r'<pre>\n?(.*?)</pre>',
               lambda m: '\n.. code-block:: text\n\n%s\n' %
                         textwrap.indent(textwrap.dedent(m.group(1).strip('\n')), '    '),
               s, flags=re.DOTALL)
    # '<ol>' is an enumerated list, '<ul>' a bulleted one. The item bodies carry
    # whatever indentation the HTML happened to use, which reStructuredText
    # would read as a nested block quote, so each item is dedented and
    # re-indented to line up under its own marker.
    def listify(m):
        marker = '#. ' if m.group(1) == 'ol' else '* '
        items = []
        for item in re.split(r'<li>', m.group(2)):
            item = re.sub(r'</li>', '', item).strip('\n')
            if not item.strip():
                continue
            lines = item.splitlines()
            head = lines[0].strip()
            tail = textwrap.dedent('\n'.join(lines[1:])).strip('\n')
            body = head + ('\n' + tail if tail else '')
            items.append(marker + textwrap.indent(body, ' ' * len(marker))
                         [len(marker):])
        return '\n\n' + '\n\n'.join(items) + '\n\n'

    s = re.sub(r'<(ul|ol)>(.*?)</\1>', listify, s, flags=re.DOTALL)
    # Stray tags left behind by unbalanced markup
    s = re.sub(r'</?[uo]l>\n?', '', s)
    s = re.sub(r'<li>\s*', '\n* ', s)
    s = re.sub(r'</li>\n?', '', s)
    return re.sub(r'\n{3,}', '\n\n', s)


def split_sections(body):
    """Split a comment body into (description, {section: [entries]})."""
    # Mark every section tag so the body can be split on them
    body = re.sub(r'^[ \t]*[\\@]param%s?\s+%s[ \t]*' % (PARAM_GROUP, CPP_GROUP),
                  MARK + 'Args\x01\\2\x01', body, flags=re.M)
    body = re.sub(r'^[ \t]*[\\@]tparam%s?\s+%s[ \t]*' % (PARAM_GROUP, CPP_GROUP),
                  MARK + 'Template Args\x01\\2\x01', body, flags=re.M)
    for pattern, name in SECTION_TAGS:
        body = re.sub(r'^[ \t]*[\\@](?:%s)[ \t]*' % pattern,
                      MARK + name + '\x01\x01', body, flags=re.M)

    parts = re.split(MARK, body)
    description, sections = parts[0], {}
    for part in parts[1:]:
        name, _, rest = part.partition('\x01')
        arg, _, text = rest.partition('\x01')
        sections.setdefault(name, []).append((arg.strip(), text))
    return description, sections


def dedent_paragraphs(lines):
    """Dedent each blank-line-separated paragraph on its own.

    A single dedent over a run of lines finds the smallest common prefix, which
    is zero as soon as one paragraph sits at column zero. Treating paragraphs
    separately keeps the rest from carrying stray indentation.
    """
    out, para = [], []
    for line in list(lines) + ['']:
        if line.strip():
            para.append(line)
        else:
            if para:
                out += textwrap.dedent('\n'.join(para)).splitlines()
            out.append('')
            para = []
    while out and not out[-1].strip():
        out.pop()
    return out


def reflow_entry(text):
    """Normalize an entry body: strip the leading blank line and dedent."""
    lines = text.strip('\n').splitlines()
    if not lines:
        return ''
    # The first line sits on the tag line itself and so has no indentation;
    # dedent the rest independently before rejoining. Only trailing newlines
    # are trimmed, so paragraph breaks inside the entry survive.
    head = lines[0].strip()
    # Dedent paragraph by paragraph. A single dedent over the whole tail finds
    # a common prefix of zero whenever the entry also contains a list, whose
    # items 'block_substitutions' has already re-indented to column zero, and
    # then leaves the prose lines carrying their original indentation.
    tail = '\n'.join(dedent_paragraphs(lines[1:])).rstrip() if lines[1:] else ''
    return head + ('\n' + tail if tail else '')


def convert_body(body):
    body = block_substitutions(body)
    description, sections = split_sections(body)
    description = inline_substitutions(description).strip('\n')

    out = [description] if description.strip() else []
    for name in SECTION_ORDER:
        entries = sections.get(name)
        if not entries:
            continue
        chunk = []
        for arg, text in entries:
            text = inline_substitutions(reflow_entry(text))
            if arg:
                first, _, rest = text.partition('\n')
                item = '%s: %s' % (arg, first.strip())
                if rest.strip():
                    item += '\n' + textwrap.indent(textwrap.dedent(rest), '    ')
                chunk.append(item)
            else:
                chunk.append(text)
        # Entries are separated by a blank line so that repeated tags stay
        # separate paragraphs. Napoleon parses either form identically, and the
        # spacing matters where the comment is read: in the header.
        out.append('%s:\n%s' % (name, textwrap.indent('\n\n'.join(chunk), '    ')))

    # Any section tag we do not model (Author, Date, ...) is left in place
    for name, entries in sections.items():
        if name not in SECTION_ORDER:
            for _, text in entries:
                out.append(inline_substitutions(reflow_entry(text)))

    return fix_roles('\n\n'.join(p.rstrip() for p in out if p.strip()))



# --- mapping C++ names onto their Python counterparts -----------------------

# Populated from the generated type stubs via --api. Cross-references written in
# C++ spelling ('Ray::maxt') or against a template name that is only bound in an
# instantiated form ('Frame' -> 'Frame3f') would not resolve otherwise.
API_NAMES = set()
API_BARE = set()    # unqualified members; Sphinx resolves these in class scope
API_DOTTED = set()  # partially qualified names, e.g. 'PixelFormat.MultiChannel'
ROLE_RE = re.compile(r'(?<!`)`([A-Za-z_][\w:.]*(?:\(\))?)`(?!`)')


def load_api_names(stub_dir):
    import ast as _ast
    from glob import glob as _glob

    def record(name, bare):
        API_NAMES.add(name)
        if bare:
            API_BARE.add(name.rsplit('.', 1)[-1])
        # Sphinx resolves a partially qualified target against the enclosing
        # class, so 'PixelFormat.MultiChannel' reaches
        # 'mitsuba.Bitmap.PixelFormat.MultiChannel'.
        parts = name.split('.')
        API_DOTTED.update('.'.join(parts[i:]) for i in range(1, len(parts) - 1))

    for f in _glob(os.path.join(stub_dir, '**', '*.pyi'), recursive=True):
        if f.endswith('_stubs.pyi'):
            continue
        try:
            tree = _ast.parse(open(f, encoding='utf-8').read())
        except SyntaxError:
            continue
        def walk(node, prefix=''):
            for child in node.body:
                if isinstance(child, _ast.ClassDef):
                    record(prefix + child.name, True)
                    walk(child, prefix + child.name + '.')
                elif isinstance(child, (_ast.FunctionDef, _ast.AsyncFunctionDef)):
                    record(prefix + child.name, not child.name.startswith('_'))
                elif isinstance(child, _ast.AnnAssign) and \
                        isinstance(child.target, _ast.Name):
                    # Enum members and typed attributes; they are referenced by
                    # name but never resolve without this.
                    record(prefix + child.target.id, False)
                elif isinstance(child, _ast.Assign):
                    for t in child.targets:
                        if isinstance(t, _ast.Name):
                            record(prefix + t.id, False)
        walk(tree)


def resolve_name(name):
    """Map a C++ cross-reference target onto a bound Python name, or None."""
    bare, parens = (name[:-2], '()') if name.endswith('()') else (name, '')
    bare = bare.replace('::', '.')
    # 'mi.' / 'mitsuba.' is the module qualifier, but 'mi' is also the
    # conventional name of a MediumInteraction, so only strip it when what
    # follows is a name the module actually exports. 'mi.wi' stays a literal.
    for pre in ('mi.', 'mitsuba.'):
        if bare.startswith(pre):
            rest = bare[len(pre):]
            if rest.partition('.')[0] in API_NAMES:
                bare = rest
            break
    if not bare:
        return None
    head, _, tail = bare.partition('.')
    # A template only bound in an instantiated form: Frame -> Frame3f
    for cand in (bare, head + '3f' + ('.' + tail if tail else '')):
        if cand in API_NAMES or (tail and cand in API_DOTTED):
            return cand + parens
        if tail and cand.split('.')[0] in API_NAMES:
            return cand + parens
    # An unqualified member name resolves against the enclosing class
    if not tail and bare in API_BARE:
        return bare + parens
    return None


def fix_roles(s):
    """Rewrite cross-references so they resolve, or demote them to literals."""
    if not API_NAMES:
        return s

    def repl(m):
        target = m.group(1)
        resolved = resolve_name(target)
        if resolved:
            return '`%s`' % resolved
        # Not part of the Python API: a C++-only type, a member access on a
        # local variable ('mi.wi'), or a parameter name. A literal is honest;
        # a role would render as a dead link.
        return '``%s``' % target
    return ROLE_RE.sub(repl, s)


# --- comment location and re-emission ------------------------------------

BLOCK_RE = re.compile(r'^([ \t]*)(/\*[*!])(.*?)\*/', re.S | re.M)
LINE_RE = re.compile(r'(?:^[ \t]*///(?!/)[^\n]*\n)+', re.M)


def strip_block(raw):
    """Turn the interior of a /** ... */ comment into a plain text body."""
    lines, indent = [], None
    for s in raw.splitlines():
        s = s.strip()
        if s.startswith('*') and not s.startswith('*/'):
            s = s[1:]
        if s.startswith(' '):
            s = s[1:]
        if s.strip():
            width = len(s) - len(s.lstrip())
            indent = width if indent is None else min(indent, width)
        lines.append(s)
    indent = indent or 0
    return '\n'.join(l[indent:] if l.strip() else '' for l in lines).strip('\n')


def emit_block(indent, body):
    out = [indent + '/**']
    for line in body.splitlines():
        out.append((indent + ' * ' + line).rstrip() if line.strip()
                   else indent + ' *')
    out.append(indent + ' */')
    return '\n'.join(out)


def emit_lines(indent, body):
    return '\n'.join((indent + '/// ' + l).rstrip() if l.strip()
                     else indent + '///' for l in body.splitlines()) + '\n'


# Doxygen member-group markers: '//! @{ \name Fields' ... '//! @}'. They label a
# run of declarations rather than documenting one, but Clang still attaches them
# to whatever follows, so they surface as docstrings reading '//! @}'. Doxygen no
# longer consumes the grouping syntax, so only the label survives as an ordinary
# comment. The closing marker is dropped entirely, leaving the banner rule that
# already follows it to delimit the region.
GROUP_OPEN_RE = re.compile(
    r'^([ \t]*)//[!/][ \t]*@\{[ \t]*\\name[ \t]*(.*?)[ \t]*$', re.M)
GROUP_CLOSE_RE = re.compile(
    r'^[ \t]*//[!/][ \t]*@\}[ \t]*\n', re.M)

def strip_group_markers(text):
    return GROUP_CLOSE_RE.sub('', GROUP_OPEN_RE.sub(r'\1// \2', text))


# Anything of the form '\word' or '@word' that survives conversion is a Doxygen
# command this script does not model. It would otherwise reach the rendered
# docs verbatim, backslash and all, which is how a stray '\author' went
# unnoticed. Collected and reported rather than guessed at.
UNKNOWN_TAG_RE = re.compile(r'(?<![\w\\])\\([a-zA-Z]{2,})\b')
# Inline literals and math roles legitimately contain backslash sequences
# (':math:`-\infty`', '``\n``'), so they are removed before scanning.
MARKUP_RE = re.compile(r'``.*?``|:[a-z:]+:`.*?`|\.\. code-block::.*?(?=\n\S|\Z)',
                       re.DOTALL)
UNRECOGNIZED = []


def report_unrecognized(path, body):
    for tag in UNKNOWN_TAG_RE.findall(MARKUP_RE.sub('', body)):
        UNRECOGNIZED.append((path, tag))


def convert_file(path, dry_run=False):
    text = open(path, encoding='utf-8').read()
    changes = [0]

    def do_block(m):
        indent, body = m.group(1), strip_block(m.group(3))
        if not DOXYGEN_RE.search(body):
            fixed = fix_roles(body)
            if fixed == body:
                return m.group(0)
            changes[0] += 1
            return emit_block(indent, fixed)
        converted = convert_body(body)
        report_unrecognized(path, converted)
        if converted == body:
            return m.group(0)
        changes[0] += 1
        return emit_block(indent, converted)

    def do_lines(m):
        raw = m.group(0)
        indent = re.match(r'[ \t]*', raw).group(0)
        body = '\n'.join(re.sub(r'^[ \t]*///[ \t]?', '', l)
                         for l in raw.rstrip('\n').splitlines())
        if not DOXYGEN_RE.search(body):
            fixed = fix_roles(textwrap.dedent(body))
            if fixed == textwrap.dedent(body):
                return raw
            changes[0] += 1
            return emit_lines(indent, fixed)
        converted = convert_body(textwrap.dedent(body))
        report_unrecognized(path, converted)
        if converted == body:
            return raw
        changes[0] += 1
        return emit_lines(indent, converted)

    result = strip_group_markers(text)
    if result != text:
        changes[0] += 1
    result = BLOCK_RE.sub(do_block, result)
    result = LINE_RE.sub(do_lines, result)

    if changes[0] and not dry_run:
        open(path, 'w', encoding='utf-8').write(result)
    return changes[0]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--dry-run', action='store_true')
    ap.add_argument('--api', metavar='<stub-dir>',
                    help='Directory of .pyi stubs used to check that '
                         'cross-reference targets actually exist')
    ap.add_argument('paths', nargs='+')
    args = ap.parse_args()
    if args.api:
        load_api_names(args.api)

    files = []
    for p in args.paths:
        if os.path.isdir(p):
            for root, _, names in os.walk(p):
                files += [os.path.join(root, n) for n in sorted(names)
                          if n.endswith(('.h', '.hpp'))]
        else:
            files.append(p)

    total = 0
    for path in files:
        n = convert_file(path, args.dry_run)
        if n:
            total += n
            print('%-60s %d comment(s)' % (os.path.relpath(path), n))
    print('\n%d comments in %d files' % (total, len(files)))

    if UNRECOGNIZED:
        import collections
        print('\nUnrecognized Doxygen commands left in the output:')
        for (path, tag), n in collections.Counter(UNRECOGNIZED).most_common():
            print('  %-52s \\%s  x%d' % (os.path.relpath(path), tag, n))
    return 0


if __name__ == '__main__':
    sys.exit(main())
