# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
#                                                                             #
#  cstylechecker.py -- Style-checker for C-source files                       #
#                                                                             #
#  Copyright (C) 2010, 2011, 2012, 2013, 2014, Synarete                       #
#                                                                             #
#  Permission is hereby granted, free of charge, to any person obtaining a    #
#  copy of this software and associated documentation files (the "Software"), #
#  to deal in the Software without restriction, including without limitation  #
#  the rights to use, copy, modify, merge, publish, distribute, sublicense,   #
#  and/or sell copies of the Software, and to permit persons to whom the      #
#  Software is furnished to do so, subject to the following conditions:       #
#                                                                             #
#  The above copyright notice and this permission notice shall be included in #
#  in all copies or substantial portions of the Software.                     #
#                                                                             #
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR #
#  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   #
#  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    #
#  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER #
#  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    #
#  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        #
#  DEALINGS IN THE SOFTWARE.                                                  #
#                                                                             #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #

import sys
import os
import re
import stat
import collections
import curses.ascii

# . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .
#
# Globals:
#
MODULE = os.path.abspath(__file__)
MODULENAME = os.path.basename(MODULE)
PROGNAME = os.path.basename(sys.argv[0])
ISATTY = os.isatty(sys.stdout.fileno())
COLORTERM = os.environ.get('COLORTERM') in ('1', 'True')
USECOLORS = os.environ.get('USECOLORS') in ('1', 'True')
USECOLORS = USECOLORS and COLORTERM and ISATTY
CLR_RED = r'\033[1;31m'
CLR_RESET = r'\033[0m'
LINELEN_MAX = 80
TOKENLEN_MAX = 40
BLOCKSIZE_MAX = 200
TAB_WIDTH = 4
LINECNT_MAX = 8000
EMPTYLINES_MAX = 6
C_HEADERS = \
    '''
        assert.h
        float.h
        math.h
        stdatomic.h
        stdlib.h
        time.h
        complex.h
        inttypes.h
        setjmp.h
        stdbool.h
        stdnoreturn.h
        uchar.h
        ctype.h
        iso646.h
        signal.h
        stddef.h
        string.h
        wchar.h
        errno.h
        limits.h
        stdalign.h
        stdint.h
        tgmath.h
        wctype.h
        fenv.h
        locale.h
        stdarg.h
        stdio.h
        threads.h
    '''
SYS_HEADERS = \
    '''
        errno.h
        signal.h
        unistd.h
        fcntl.h
        poll.h
        prctl.h
        pthread.h
        sys/types.h
        sys/stat.h
        sys/time.h
        sys/select.h
    '''

UNSAFE_FUNCS = \
    '''
        getdents
        sprintf
        scalbf
    '''

NON_REENTRANT_FUNCS = \
    '''
        crypt
        encrypt
        getgrgid
        getgrnam
        getlogin
        getpwnam
        getpwuid
        asctime
        ctime
        gmtime
        localtime
        getdate
        rand
        random
        readdir
        strtok
        ttyname
        hcreate
        hdestroy
        hsearch
        getmntent
    '''

WRAPPER_FUNCS = \
    '''
        assert
        bzero
        usleep
        min
        max
    '''

COMPILER_PRIVATE = \
    '''
        __builtin_
        __asm
        __sync
        __file__
        __line__
        __func__
        __inline__
        __attribute__
        __extension__
        __typeof__
        __clang__
        __aligned__
        __packed__
        __pure__
        __nonnull__
        __noreturn__
        __cplusplus
        __has_feature
        __thread
        __FILE__
        __LINE__
        __TIME__
        __DATE__
        __COUNTER__
        __OPTIMIZE__
        __VA_ARGS__
        __BYTE_ORDER
        __WORDSIZE
        __GNUC__
        __GNUC_MINOR__
        __GNUC_PATCHLEVEL__
        __USE_GNU
        __INTEL_COMPILER
        __i386__
        __x86_64__
    '''

SYS_PRIVATE = \
    '''
        __STDC__
        __LITTLE_ENDIAN
        __BIG_ENDIAN
        __u8 __u16 __u32 __u64
         __s8 __s16 __s32 __s64
        __le16 __le32 __le64
        __be16 __be32 __be64
        __rlimit_resource_t
    '''

CSOURCE_EXCLUDE = \
    '''
        extern
        true
        false
        bool
        new
        delete
        private
        protected
        public
        using
        namespace
        cplusplus
        _cast
        try
        throw
        catch
        mutable
        friend
        template
        virtual
        operator
        setjmp
        longjmp
    '''

RESERVED_TOKENS = \
    '''+-
       -+
       ''
       ~~
       !!
       ??
       ---
       +++
       &&&
       ***
       <<<
       >>>
       ___
       ===
       \\\\
       ////
       (((((
       )))))
    '''

RE_FUNC_DECL = re.compile(r'''\w+ \w+\(.*\);$''')
RE_SIZEOF_ADDRESS = re.compile(r'''\bsizeof\s*\(\s*\&''')
RE_SUSPICIOUS_SEMICOLON = re.compile(r'''\bif\s*\(.*\)\s*;''')
RE_C_COMMENT = re.compile(r'''/\*.*?\*/''', re.DOTALL)
RE_CPP_COMMENT = re.compile(r'''//.*?\n''')


# . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .


def is_reg(path):
    try:
        return stat.S_ISREG(os.stat(path).st_mode)
    except (NameError, OSError):
        pass


def is_cfile(path):
    ext = os.path.splitext(path)[1]
    return str(ext) in ('.c') and is_reg(path)


def is_hfile(path):
    ext = os.path.splitext(path)[1]
    return len(ext) and (ext in ('.h', '.hpp', '.hxx')) and is_reg(path)


def is_srcfile(path):
    return is_reg(path) and (is_cfile(path) or is_hfile(path))


# . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .

def tokenize(line):
    '''Splits line into tokens: converts strings and delimiters to spaces,
       then split into tokens.
    '''
    wline = ''
    instr = False
    for c in line.rstrip():
        if c == '"' and not instr:
            instr = True
        if instr:
            c = ' '
        wline = wline + c
        if c == '"' and instr:
            instr = False
    for c in ' { * } [ ] ( ) ; : . '.split():
        wline = wline.replace(c, ' ')
    return wline.split()

# . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .


class Context:
    '''Checkers context object'''
    error_count = 0

    def __init__(self, path='', line_no=0, line='', lines=None):
        self.path = path
        self.line_no = line_no
        self.line = line
        self.lines = lines
        self.xlines = []
        self.zlines = []
        self.tokens = tokenize(line)
        self.iscfile = is_cfile(path)
        self.ishfile = is_hfile(path)
        if lines:
            self.zlines = lines_nums_with_comments(lines)
            self.xlines = lines_nums_no_comments(lines)

    def logmsg(self, msg, lnum=None):
        if not lnum:
            lnum = self.line_no
        print("{0} {1}".format(self.msg_prefix(self.path, lnum), msg))

    def error(self, msg, lnum=None):
        self.logmsg('Error: ' + msg, lnum)
        self.incerr(1)

    def warn(self, msg, lnum=None):
        self.logmsg('Warning: ' + msg, lnum)
        self.incerr(1)

    def info(self, msg, lnum=None):
        self.logmsg(msg, lnum)

    @classmethod
    def incerr(cls, n=1):
        cls.error_count += n

    @staticmethod
    def msg_prefix(path, line_no):
        s = ''
        if path:
            s = s + os.path.relpath(path) + ':'
        if line_no:
            s = s + str(line_no) + ':'
        if PROGNAME:
            s = str(PROGNAME) + ': ' + s
        elif MODULENAME:
            s = str(MODULENAME) + ': ' + s
        if USECOLORS:
            s = CLR_RED + s + CLR_RESET
        return s


# . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .


def has_mixed_case(tok):
    '''Returns True if a token consists of mixed upper/lower characters
       If a token is a combination of two or more sub-tokens (e.g., system
       defines such as SYS_gettid), check each sub-token.
    '''
    if '_' in tok:
        for t in tok.split('_'):
            if has_mixed_case(t):
                return True
    else:
        (has_lower, has_upper) = (False, False)
        for c in tok:
            if c.islower():
                has_lower = True
            if c.isupper():
                has_upper = True
            if has_lower and has_upper:
                return True
    return False


def using_function(line, fn):
    '''Checks if function call exists in line'''
    return fn + '(' in line or fn + ' (' in line


def lines_nums(lines):
    '''Generators of (line-number, line) tuples list; either with comments
    (raw) or without (xline)
    '''
    return zip(lines, range(1, len(lines) + 1))


def lines_nums_with_comments(lines):
    return lines_nums(lines)


def restrip_comments(txt):
    '''Remove all occurrence streamed comments (/*COMMENT */) from string
       Remove all occurrence single-line comments (//COMMENT\n ) from string
    '''
    s = str(txt)
    s = re.sub(RE_C_COMMENT, "", s)
    s = re.sub(RE_CPP_COMMENT, "", s)
    return s


def lines_nums_no_comments(lines):
    ls = []
    in_comment = False
    for (ln, no) in lines_nums(lines):
        if '''/*''' in ln:
            in_comment = True
        if '''*/''' in ln:
            in_comment = False
        if not in_comment:
            ln2 = restrip_comments(ln)
            ls.append((ln2, no))
    return ls


# . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .


def check_line_length(ctx):
    '''Source-code lines should not be more then 80 chars long. Map TABs to
       4 white space characters.
    '''
    line_len = len(ctx.line.replace('\t', ' ' * TAB_WIDTH).rstrip())
    if line_len > LINELEN_MAX:
        ctx.error("Long-line len={0}".format(line_len))


def check_no_cxx_comments(ctx):
    '''Avoid C++ style comments (be as portable as possible)'''
    line = ctx.line
    if line.strip().startswith('//') or line.find(' //') >= 0:
        ctx.logmsg("C++ comment")


def check_ascii_printable(ctx):
    '''All characters should be ASCII-printable'''
    for c in ctx.line.strip():
        if not curses.ascii.isprint(c) and (c != '\t'):
            ctx.error("Non-ASCII-printable ord={0}".format(ord(c)))


def check_only_indent_tabs(ctx):
    '''Source line should have no tabs, except for indent'''
    line = ctx.line
    tcol = max(line.rfind('\t'), line.rfind('\v'))
    line2 = line.lstrip()
    tcol2 = max(line2.rfind('\t'), line2.rfind('\v'))
    if tcol >= 0 and tcol2 >= 0:
        ctx.logmsg("Tab-character at pos={0}".format(tcol))


# . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .


def check_no_multi_semicolon(ctx):
    '''Should not have multiple ;; '''
    ln = ctx.line.rstrip()
    i = ln.rfind(';;')
    if i >= 0:
        ctx.logmsg("Multiple semi-colon")


def check_no_long_tokens(ctx):
    '''Avoid having loooooong tokens'''
    for tok in ctx.tokens:
        if len(tok) > TOKENLEN_MAX:
            ctx.error("Long token: {0}".format(tok))


def check_no_suspicious_semicolon(ctx):
    '''Avoid having semicolon at the end of if'''
    ln = ctx.line.strip()
    if re.search(RE_SUSPICIOUS_SEMICOLON, ln) is not None:
        ctx.logmsg("Suspicious semicolon")


def check_no_relative_include(ctx):
    '''Should not have relative includes'''
    ln = ctx.line.strip()
    (i, j) = (ln.find('#include'), ln.find('../'))
    if (i == 0) and (j > 0):
        ctx.error("Relative path in: {0}".format(ln))


def check_no_sizeof_address(ctx):
    '''Avoid using sizeof(&)'''
    if re.search(RE_SIZEOF_ADDRESS, ctx.line) is not None:
        ctx.error("Avoid sizeof(& ")


def check_struct_union_name(ctx):
    '''Names of struct/union should be all lower'''
    check = False
    for tok in ctx.tokens:
        if check and not tok.islower():
            ctx.logmsg("Non-valid-name {0}".format(tok))
        check = ((tok == 'struct') or (tok == 'union'))


def is_typename(name):
    for t in name.split('_'):
        if not t.isalnum() or not t[0].isalpha():
            return False
    return True


def check_no_mixed_case(ctx):
    '''Names of function/struct/union/variable must be all lower/upper case'''
    names_tokens = [tok for tok in ctx.tokens if is_typename(tok)]
    for tok in names_tokens:
        if has_mixed_case(tok):
            ctx.logmsg("Mixed-case: '{0}'".format(tok))


def is_private(tok):
    compiler_private = COMPILER_PRIVATE.split()
    sys_private = SYS_PRIVATE.split()
    for p in compiler_private:
        if tok.startswith(p):
            return True
    for p in sys_private:
        if tok.startswith(p):
            return True
    return False


def check_underscore_prefix(ctx):
    '''Double-underscore prefix should be reserved for compiler'''
    for tok in ctx.tokens:
        if (len(tok) > 2) and tok.startswith('__'):
            if not is_private(tok):
                ctx.logmsg("Not a compiler/system built-in {0}".format(tok))


def check_no_unsafe_functions(ctx):
    '''Avoid unsafe/obsolete functions'''
    unsafe_funcs = UNSAFE_FUNCS.split()
    for fn in unsafe_funcs:
        if fn in ctx.tokens and using_function(ctx.line, fn):
            ctx.logmsg("Unsafe-function: '{0}'".format(fn))


def check_no_non_reentrant_func(ctx):
    '''Avoid non-reentrant functions'''
    non_reentrant_funcs = NON_REENTRANT_FUNCS.split()
    for fn in non_reentrant_funcs:
        if fn in ctx.tokens and using_function(ctx.line, fn):
            ctx.logmsg("Non-reentrant {0} (use: {0}_r)".format(fn))


def check_using_wrapper_functions(ctx):
    '''Prefer wrapper functions'''
    wrapper_funcs = WRAPPER_FUNCS.split()
    for fn in wrapper_funcs:
        if fn in ctx.tokens and using_function(ctx.line, fn):
            ctx.logmsg("Prefer wrapper function: '{0}'".format(fn))


def check_no_excluded_keyword(ctx):
    '''Avoid using some (C++) keywords in C files'''
    csource_exclude = CSOURCE_EXCLUDE.split()
    for ex in csource_exclude:
        word = ex.strip(".-+%~><?^*")
        if word in ctx.tokens:
            ctx.logmsg("Avoid using '{0}' in C files".format(word))


def check_no_reserved_tokens(ctx):
    '''Avoid using reserved tokens which may confuse'''
    reserved_tokens = RESERVED_TOKENS.split()
    for rt in reserved_tokens:
        tok1 = ' ' + rt
        tok2 = rt + ' '
        if tok1 in ctx.line or tok2 in ctx.line:
            ctx.logmsg("Avoid using '{0}'".format(rt))


def check_no_static_inline(ctx):
    '''Avoid using 'static inline ' function declaration within C source file;
       Let the compiler be wise for us.
    '''
    if 'static inline ' in ctx.line:
        ctx.logmsg("Avoid using 'static inline' in C source file")

# . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .


def check_line_style(ctx):
    check_line_length(ctx)
    check_no_cxx_comments(ctx)
    check_ascii_printable(ctx)
    check_only_indent_tabs(ctx)


def check_xline_style(ctx):
    check_no_multi_semicolon(ctx)
    check_no_long_tokens(ctx)
    check_no_suspicious_semicolon(ctx)
    check_no_relative_include(ctx)
    check_no_sizeof_address(ctx)
    check_struct_union_name(ctx)
    check_no_mixed_case(ctx)
    check_underscore_prefix(ctx)
    check_no_unsafe_functions(ctx)
    check_no_non_reentrant_func(ctx)
    check_using_wrapper_functions(ctx)
    check_no_reserved_tokens(ctx)
    if ctx.iscfile:
        check_no_excluded_keyword(ctx)
        check_no_static_inline(ctx)

# . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .


def check_file_lines_cnt(ctx):
    '''Check limit number of lines per file'''
    lncnt = len(ctx.xlines)
    if lncnt > LINECNT_MAX:
        ctx.error("Too-many source lines: {0}".format(lncnt))


def check_block_size(ctx):
    '''Check block-sizes within { and } is not larger then BLOCKSIZE_MAX'''
    deque = collections.deque()
    for (ln, no) in ctx.xlines:
        for c in ln:
            if c == '{':
                deque.append(no)
            if c == '}':
                dif = 0
                try:
                    no0 = deque.pop()
                    dif = no - no0
                except IndexError:
                    ctx.logmsg("Block-error")
                if dif > BLOCKSIZE_MAX:
                    ctx.logmsg("Block-overflow sz={0}".format(dif), lnum=no0)


def starts_with_ppg(ln, tok):
    '''Check pre-processing guards match filename for .h files'''
    s = ln.lstrip('#').strip()
    t = tok.lstrip('#').strip()
    return s.startswith(t)


def check_pps_guards(ctx):
    xlines = ctx.xlines
    name = os.path.split(ctx.path)[1]
    guard = name.upper().replace('.', '_').replace('-', '_') + '_'

    ls1 = [(ln, no) for (ln, no) in xlines if starts_with_ppg(ln, 'define')]
    ls2 = [(ln, no) for (ln, no) in xlines if starts_with_ppg(ln, 'ifndef')]
    count1 = len([ln for (ln, no) in ls1 if guard in ln])
    count2 = len([ln for (ln, no) in ls2 if guard in ln])
    if (count1 != count2) or (count1 != 1):
        line_no = 0
        if len(ls1):
            line_no = ls1[0][1]
        elif len(ls2):
            line_no = ls2[0][1]
        ctx.logmsg("Pre-processing guard (use: {0})".format(guard), line_no)


def check_lspace_between_func_decl(ctx):
    '''Ensure at least single blank line between two functions declarations'''
    decl = 0
    for (ln, no) in ctx.xlines:
        if re.match(RE_FUNC_DECL, ln) is not None:
            decl = decl + 1
        else:
            decl = 0
        if decl >= 2:
            ctx.logmsg("No space between functions declarations", no)


def check_includes_nodup(ctx):
    '''Ensure no duplicated includes, have right order'''
    includes = {}
    for (ln, no) in ctx.xlines:
        if ln.strip().startswith('#include'):
            inc = ln.split()[1]
            inc = inc.strip('"<>')
            if inc not in includes:
                includes[inc] = [(ln, no)]
            else:
                includes[inc].append((ln, no))
    for i in includes.iterkeys():
        inc_i = includes[i]
        if len(inc_i) > 1:
            (ln, no) = inc_i[1]
            ctx.logmsg("Multi include '{0}'".format(i), no)


def check_includes_std(ctx):
    '''STD headers include must be with <>'''
    std_headers = (C_HEADERS + SYS_HEADERS).split()
    for (ln, no) in ctx.xlines:
        if ln.strip().startswith('#include'):
            inc = ln.split()[1]
            hdr = inc.strip('"<>')
            if hdr in std_headers and inc.startswith('"'):
                ctx.logmsg("Malformed include: '{0}'".format(hdr), no)


def check_includes_order(ctx):
    '''Ensure system includes come first'''
    sys_includes = True
    for ln_no in ctx.xlines:
        ln = ln_no[0]
        if ln.strip().startswith('#include'):
            inc = ln.split()[1]
            if inc.startswith('"') and not 'config.h' in inc:
                sys_includes = False
            elif inc.startswith('<') and not sys_includes:
                ctx.logmsg("Wrong include order: '#include {0}'".format(inc))


def check_includes_suffix(ctx):
    '''Should not have non-headers includes'''
    for ln_no in ctx.xlines:
        ln = ln_no[0]
        if ln.strip().startswith('#include'):
            inc = ln.split()[1]
            inc = inc.strip('<">')
            if not inc.endswith('.h'):
                ctx.logmsg("Wrong header suffix: '{0}'".format(inc))


def check_enum_def(ctx):
    '''Enum-names should be all upper-case + underscores'''
    enum_lines = []
    in_enum_def = False
    for (ln, no) in ctx.xlines:
        if in_enum_def:
            enum_lines.append((ln, no))
        elif ln.startswith('enum ') or ' enum ' in ln:
            in_enum_def = True
        if ('}' in ln) or (';' in ln):
            in_enum_def = False
    for (ln, no) in enum_lines:
        toks = ln.strip("{[()]} \t\r\v\n").split()
        if len(toks):
            t = toks[0].strip('=;:')
            if len(t) and t.isalnum() and not t.isupper():
                ctx.logmsg("Illegal enum-name {0}".format(t), lnum=no)


def check_empty_lines(ctx):
    '''Limit the number of consecutive empty lines'''
    cnt = 0
    lnum = 0
    limit = EMPTYLINES_MAX
    for (ln, no) in ctx.zlines:
        if len(ln.strip()) == 0:
            cnt = cnt + 1
            if cnt == limit:
                lnum = no
        else:
            if (lnum != 0):
                msg = "Too many empty lines (max={0})".format(limit)
                ctx.logmsg(msg, lnum=lnum)
            cnt = lnum = 0


def check_close_braces(ctx):
    '''Avoid empty lines before close-braces'''
    empty_line = False
    for (ln, no) in ctx.zlines:
        sln = ln.strip()
        if len(sln) == 0:
            empty_line = True
        if sln == '}' and empty_line:
            ctx.logmsg("Closing brace after empty line", lnum=no)
        if len(sln) != 0:
            empty_line = False

# . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .


def check_lines_style(ctx):
    '''Style-checker per single (raw) line'''
    for (ln, no) in ctx.zlines:
        lctx = Context(path=ctx.path, line_no=no, line=ln)
        check_line_style(lctx)


def check_xlines_style(ctx):
    '''Style-checker per single (processed) line'''
    for (ln, no) in ctx.xlines:
        lctx = Context(path=ctx.path, line_no=no, line=ln)
        check_xline_style(lctx)


def check_file_style(ctx):
    '''Style checker for all (processed) lines in file'''
    check_file_lines_cnt(ctx)
    check_includes_nodup(ctx)
    check_includes_std(ctx)
    check_includes_order(ctx)
    check_includes_suffix(ctx)
    check_enum_def(ctx)
    check_block_size(ctx)
    check_empty_lines(ctx)
    check_close_braces(ctx)
    if ctx.ishfile:
        check_pps_guards(ctx)
        # check_lspace_between_func_decl(ctx)

# . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .


def checkcstyleof(filename):
    '''Check C-style for C source/header file'''
    lines = file(filename, 'r').readlines()
    ctx = Context(path=filename, lines=lines)

    check_lines_style(ctx)
    check_xlines_style(ctx)
    check_file_style(ctx)


def checkcstyle(files):
    '''Check C-style for inpput C files'''
    Context.error_count = 0
    for f in files:
        if is_srcfile(f):
            checkcstyleof(f)
    return Context.error_count


if __name__ == '__main__':
    checkcstyle(sys.argv[1:])

# cstylechecker.py

