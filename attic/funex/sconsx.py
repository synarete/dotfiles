#
# sconsx.py -- SCons Extensions
#
# Copyright (C) 2010, 2011, 2012, 2013, 2014, 2015 -- Syanrete
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sub-license,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
#

'''sconsx.py

SCons eXtensions: smart wrapper over SCons.Environment.
Supports the following command line arguments:
    DEBUG=0..2               Debug level; default=1
    OPTLEVEL=0..3            Optimize level; default=0
    CC=<cc-name>             C compiler path|name; default=gcc
    CXX=<cxx-name>           C++ compiler path|name; default=g++
    BUILDDIR=<dir>           Build sub directory; default=_build
    SILENT=0|1               Short-output-format; default=on

(see 'getvars' below for full list of supported command-line variables)

Provides:
  * Strict compiler flags
  * Support for gcc & clang
  * Custom build/dist dir
  * Extended methods (ProgramRun, AppendLibPath etc).
  * Pretty output

Usage:
Place sconsx.py within your project's source code and add the following lines
at the beginning of top-level SConstruct:

    import sys
    sys.path.append(<PATH-TO-SCONSX-DIR>)
    import sconsx

    # Creation:
    env = sconsx.Environment()
    Export('env')

    # Normal usage:
    env.StaticLibrary()
    env.Zip()

    # Extended usage:
    env.AppendIncludePath('include')
    env.ProgramRun(prog)

'''

import os
import getpass
import inspect
import datetime
import subprocess

# . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .
#
# SCons':
#
try:
    import SCons.Script
except ImportError as e:
    print("** Not within SConstruct/SConscript ")
    raise e

Split = SCons.Script.Split
AddMethod = SCons.Script.AddMethod
Variables = SCons.Script.Variables
BoolVariable = SCons.Script.BoolVariable
EnumVariable = SCons.Script.EnumVariable
Action = SCons.Action.Action
Builder = SCons.Builder.Builder
ARGUMENTS = SCons.Script.ARGUMENTS


# . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .
#
# Extend and override user provided command line variables.
#
def getvar_user():
    return getpass.getuser()


def getvar_datetime():
    # return str(time.strftime("%c", time.localtime()))
    now = datetime.datetime.now()
    return now.isoformat().split('.')[0].rsplit(':', 1)[0]


def getvar_sysname():
    return str(os.uname()[1])


def getvar_sysvers():
    return str(os.uname()[0]) + '-' + str(os.uname()[2])


def getvar_version():
    key = 'VERSION'
    ver = os.environ.get(key, '')
    try:
        with open(key, 'r') as f:
            ver = f.readlines()[0]
    except:
        pass
    return ver.strip()


def getvar_release():
    key = 'RELEASE'
    rel = os.environ.get(key, '1')
    return rel.strip()


def getvar_revision():
    try:
        cmd = 'git rev-parse --short=10 HEAD'
        rev = subprocess.check_output(cmd.split())
    except subprocess.CalledProcessError:
        rev = ''
    return rev.strip()


def validate_cc(key, val, env):
    if 'gcc' not in val and 'clang' not in val:
        raise Exception("Invalid {0} value: '{1}'".format(key, val))


def validate_cxx(key, val, env):
    if not val in ('g++', 'clang++'):
        raise Exception("Invalid {0} value: '{1}'".format(key, val))


def format_var_helptext(env, opt, hlp, default, actual, a):
    return '  {0: <11}: {1}\n'.format(opt, actual)


def getvars():
    vrs = Variables(None, ARGUMENTS)
    vrs.AddVariables(
        ('USER', 'Username', getvar_user()),
        ('DATETIME', 'Datetime', getvar_datetime()),
        ('SYSNAME', 'Sysname', getvar_sysname()),
        ('SYSVERS', 'Sysvers', getvar_sysvers()),
        ('VERSION', 'Version', getvar_version()),
        ('RELEASE', 'Release', getvar_release()),
        ('REVISION', 'Revision', getvar_revision()),
        ('CC', 'C Compiler', 'gcc', validate_cc),
        ('CXX', 'C++ Compiler', 'g++', validate_cxx),
        (EnumVariable('DEBUG', 'Debug', '1', '0 1 2 3')),
        (EnumVariable('OPTLEVEL', 'Optimization', '0',
                      ' '.join(GCC_OPTLEVEL_FLAGS.keys()))),
        (BoolVariable('SILENT', 'Silent', True)),
        (BoolVariable('COVERAGE', 'Code coverage', False)),
        ('BUILDDIR', 'Build sub-dir', '_build'))
    vrs.FormatVariableHelpText = format_var_helptext
    return vrs


def is_inherit_wanted(name, value):
    '''Helper function to filter out unwanted members for inheritance'''
    return len(name) and (name.find('_') == -1) \
        and name[0].isupper() and callable(value)


def make_builder_entry(action_name, exec_cmd, src_suffix='', suffix='',
                       single_source=True, ensure_suffix=True):
    '''Helper for construction input to SCons builder'''
    com = action_name + 'COM'
    comstr = action_name + 'COMSTR'
    action = Action('$' + com, '$' + comstr)
    builder = Builder(action=action,
                      suffix=suffix,
                      src_suffix=src_suffix,
                      single_source=single_source,
                      ensure_suffix=ensure_suffix)
    return (com, comstr, builder, exec_cmd)


class Environment:

    '''Extended wrapper over SCons' Environment, with additional customizations

        - Command-line args (debug, optlevel).
        - Use Compiler pedantic flags.
        - Custom builders (rst).
        - Force 'build' sub-dir and no source duplication.
        - Pretty-output (optional, colors).
        - Supported compilers: GCC (GNU), CLANG (LLVM)
    '''

    def __init__(self, variables=None, sconsenv=None):
        self.builddir = None
        self.vars = variables
        self.env = sconsenv
        if variables is None:
            self.vars = getvars()
        if sconsenv is None:
            self.set_new_environment()

        self.init_builddir_dir()
        self.init_inherited_members()
        self.set_compiler_config_flags()
        self.set_extended_builders()
        self.init_extended_methods()
        self.init_override_methods()
        self.init_output_format()

    def __getitem__(self, key):
        return self.env.__getitem__(key)

    def __setitem__(self, key, value):
        return self.env.__setitem__(key, value)

    def set_new_environment(self):
        self.env = SCons.Script.Environment(variables=self.vars,
                                            ENV=os.environ,
                                            CPPPATH=[],
                                            LIBPATH=[],
                                            LIBS=[],
                                            CCFLAGS=[],
                                            LINKFLAGS=[])

    def init_builddir_dir(self):
        if self.builddir is None:
            bdir = '#' + self.env['BUILDDIR']
        else:
            bdir = self.builddir
        self.env.VariantDir(variant_dir=bdir, src_dir='#', duplicate=0)
        self.builddir = bdir

    def init_inherited_members(self):
        env = self.env
        for (name, value) in inspect.getmembers(env):
            if is_inherit_wanted(name, value):
                setattr(self, name, getattr(env, name))

    def init_extended_methods(self):
        self.add_method(self.sub_sconscript, 'SubSConscript')
        self.add_method(self.sub_sconscript_lib, 'SubSConscriptLib')
        self.add_method(self.append_libpath, 'AppendLibPath')
        self.add_method(self.append_includepath, 'AppendIncludePath')
        self.add_method(self.append_includelibpath, 'AppendIncludeLibPath')
        self.add_method(self.program_run, 'ProgramRun')
        self.add_method(self.program_exec, 'ProgramExec')
        self.add_method(self.program_exec2, 'ProgramExec2')
        self.add_method(self.unit_test, 'UnitTest')
        self.add_method(self.rst2html, 'Rst2Html')
        self.add_method(self.rst2man, 'Rst2Man')
        self.add_method(self.show_vars, 'ShowVars')

    def init_override_methods(self):
        self.add_method(self.clone, 'Clone')
        self.add_method(self.program, 'Program')

    def init_output_format(self):
        if self.issilent():
            set_silent_output(self)

    def issilent(self):
        return self['SILENT']

    def add_method(self, method, name):
        AddMethod(self, method, name)

    def sub_sconscript(self, d, include=False, lib=False):
        env = self.env
        res = []
        for dd in self.flatten(d):
            vd = self.variant_dir_of(dd)
            sd = '#' + self.source_dir_of(dd)
            if include:
                self.append_includepath(sd)
            if lib:
                self.append_libpath(vd)
            rr = env.SConscript(dirs=sd, variant_dir=vd, duplicate=False)
            res.append(rr)
        return res

    def sub_sconscript_lib(self, d):
        return self.sub_sconscript(d, include=True, lib=True)

    def variant_dir_of(self, d):
        bd = self.builddir
        dd = self.source_dir_of(d)
        vd = str(os.path.join(bd, str(dd)))
        return vd

    def source_dir_of(self, d):
        env = self.env
        dd = env.Dir(d).srcnode().path
        return str(dd)

    def append_libpath(self, d):
        env = self.env
        res = []
        for dd in self.flatten(d):
            sd = '#' + self.source_dir_of(dd)
            vd = self.variant_dir_of(sd)
            lp = env.AppendUnique(LIBPATH=env.Dir(vd))
            res.append(lp)
        return res

    def append_includepath(self, d):
        env = self.env
        res = []
        for dd in self.flatten(d):
            inp = '#' + self.source_dir_of(dd)
            inp = env.AppendUnique(CPPPATH=env.Dir(inp))
            res.append(inp)
        return res

    def append_includelibpath(self, d):
        self.append_libpath(d)
        self.append_includepath(d)

    def unit_test(self, src, libs, args=''):
        xlibs = self.getxlibs(libs)
        return self.program_run(src, xlibs, args)

    def program_run(self, src, libs, args=''):
        env = self.env
        tgt = '${SOURCE.filebase}'
        prog = env.Program(target=tgt, source=src, LIBS=libs)
        if args and len(str(args)) > 0:
            rc = self.program_exec(prog, args)
        else:
            rc = self.program_exec2(prog)
        return rc

    def program_exec(self, prog, args=''):
        env = self.env
        tgt = '${SOURCE.filebase}.out'
        cmd = '$SOURCE ' + str(args) + ' > $TARGET'
        if self.issilent():
            cst = make_silent_output_str(self, 'RUN', True, False, '')
            cst = cst + ' ' + str(args)
            act = Action('@' + cmd, cmdstr=cst)
        else:
            act = Action(cmd)
        return env.Command(tgt, prog, act)

    def program_exec2(self, prog):
        return self.env.ExecuteProgram(prog)  # Custom builder

    def rst2html(self, source):
        res = []
        for s in self.flatten(source):
            exe = self.env.ExecuteRst2Html(s)
            res.append(exe)
        return res

    def rst2man(self, source):
        res = []
        for s in self.flatten(source):
            exe = self.env.ExecuteRst2Man(s)
            res.append(exe)
        return res

    def flatten(self, d):
        return self.env.Flatten(d)

    def clone(self):
        return Environment(self.vars, self.env.Clone())

    def program(self, target, source, libs=None, ccflags=None):
        xlibs = self.getxlibs(libs)
        xccflags = self.env['CCFLAGS']
        if ccflags:
            xccflags = ccflags + xccflags
        return self.env.Program(target=target, source=source, \
                                LIBS=xlibs, CCFLAGS=xccflags)

    def getxlibs(self, libs=None):
        xlibs = self.env['LIBS']
        if libs:
            xlibs = libs + xlibs
        return xlibs

    def set_compiler_config_flags(self):
        '''Assign compiler flags, specific-custom per known compiler.'''
        (cflags, ccflags, cxxflags, linkflags, cclibs) = self.compiler_config()
        env = self.env
        env.AppendUnique(CFLAGS=cflags)
        env.AppendUnique(CCFLAGS=ccflags)
        env.AppendUnique(CXXFLAGS=cxxflags)
        env.AppendUnique(LINKFLAGS=linkflags)
        env.AppendUnique(LIBS=cclibs)

    def compiler_config(self):
        '''Returns a tuple of compiler/linker configuration flags:
            (CFLAGS, CCFLAGS, CXXFLAGS, LINKFLAGS)
        '''
        cc = self.compiler_name()
        if 'gcc' in cc:
            return self.compiler_config_gcc()
        elif 'clang' in cc:
            return self.compiler_config_clang()
        else:
            raise Exception("Unsupported compiler {0}".format(cc))

    def compiler_config_gcc(self):
        (name, versnum, debug, optlevel, cov) = self.compiler_params()

        c_flags = Split(GCC_CFLAGS)
        cc_flags = Split(GCC_CCFLAGS)
        cxx_flags = Split(GCC_CXXFLAGS)
        link_flags = Split(GCC_LINKFLAGS)
        cc_flags += Split(GCC_OPTLEVEL_FLAGS[optlevel])
        cc_libs = []
        if int(versnum) >= 47:
            c_flags += Split(GCC_CFLAGS_EXTRA)
            cc_flags += Split(GCC_CCFLAGS_EXTRA)
        if int(debug) > 0:
            cc_flags += Split(GCC_DEBUG_FLAGS % int(debug))
            cc_flags += Split(GCC_SECURITY_FLAGS)
        if int(optlevel) == 0:
            cc_flags += Split(GCC_OPTLEVEL0_FLAGS)
        if cov:
            cc_flags += Split(GCC_GCOV_CCFLAGS)
            link_flags += Split(GCC_GCOV_LINKFLAGS)
            cc_libs += Split(GCC_GCOV_LIBS)
        return (c_flags, cc_flags, cxx_flags, link_flags, cc_libs)

    def compiler_config_clang(self):
        (name, versnum, debug, optlevel, cov) = self.compiler_params()

        c_flags = Split(CLANG_CFLAGS) + Split(CLANG_CFLAGS_EXTRA)
        cc_flags = Split(CLANG_CCFLAGS) + Split(CLANG_CCFLAGS_EXTRA)
        cxx_flags = Split(CLANG_CXXFLAGS)
        link_flags = Split(CLANG_LINKFLAGS)
        cc_flags += Split(CLANG_OPTLEVEL_FLAGS[optlevel])
        if int(debug) > 0:
            cc_flags += Split(CLANG_DEBUG_FLAGS % int(debug))
            cc_flags += Split(CLANG_SECURITY_FLAGS)
        return (c_flags, cc_flags, cxx_flags, link_flags, [])

    def compiler_params(self):
        '''Returns compiler params which effect config'''
        name = self.compiler_name()
        versnum = self.compiler_versnum()
        debug = self.env['DEBUG']
        optlevel = self.env['OPTLEVEL']
        cov = self.env['COVERAGE']
        return (name, versnum, debug, optlevel, cov)

    def compiler_versnum(self):
        '''Returns compiler version as decimal number'''
        ccversion = self.env['CCVERSION']
        v = [int(z) for z in ccversion.split('.')]
        (major, minor) = (tuple(v) + (0, 0))[0:2]
        return ((10 * int(major)) + int(minor))

    def compiler_name(self):
        '''Return compiler's striped name from path'''
        return os.path.basename(self.env['CC'])

    def set_extended_builders(self):
        self.add_rst2man_builder()
        self.add_rst2html_builder()
        self.add_execprog_builder()

    def add_rst2man_builder(self):
        bname = 'ExecuteRst2Man'
        command = 'rst2man $SOURCE | gzip > $TARGET'
        builder = make_builder_entry('RST2MAN', command,
                                     src_suffix='.txt', suffix='.gz')
        self.add_custom_builder(bname, builder)

    def add_rst2html_builder(self):
        bname = 'ExecuteRst2Html'
        command = 'rst2html $SOURCE > $TARGET'
        builder = make_builder_entry('RST2HTML', command,
                                     src_suffix='.txt', suffix='.html')
        self.add_custom_builder(bname, builder)

    def add_execprog_builder(self):
        bname = 'ExecuteProgram'
        command = '$SOURCE > $TARGET'
        builder = make_builder_entry('EXEC', command, suffix='.out')
        self.add_custom_builder(bname, builder)

    def add_custom_builder(self, bname, builder):
        env = self.env
        (com, comstr, builder, exec_cmd) = builder
        env[com] = exec_cmd
        # env[comstr] = None
        env.Append(BUILDERS={bname: builder})

    def show_vars(self):
        print('')
        print(self.vars.GenerateHelpText(self.env))

# . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .


def set_silent_output(env):
    '''Configure silent-output formatting for enviroement's actions:
        - Do short output for compilation lines.
        - Optinaly, use colors.
    '''
    set_silent_output_builtin_tools(env)
    set_silent_output_aspp(env)
    set_silent_output_install(env)
    set_silent_output_execprog(env)
    set_silent_output_rst2man(env)
    set_silent_output_rst2html(env)


def set_silent_output_builtin_tools(env):
    builtin_tools = ['AR', 'AS', 'CC', 'CXX', 'LINK',
                     'RANLIB', 'SHCC', 'SHCXX', 'SHLINK']
    for t in builtin_tools:
        set_silent_output_tool(env, t)


def set_silent_output_tool(env, tool):
    p = '{0} ({1})'.format(tool, env[tool])
    p = '{0}'.format(tool)
    s = make_silent_output_str(env, p, src=False, tgt=True)
    set_silent_output_str(env, tool, s)


def set_silent_output_aspp(env):
    set_silent_output_str(env, 'ASPP')


def set_silent_output_install(env):
    t = 'INSTALL'
    s = make_silent_output_str(env, t)
    set_silent_output_str(env, t, s)


def set_silent_output_execprog(env):
    t = 'EXEC'
    s = make_silent_output_str(env, t, tgt=False)
    set_silent_output_str(env, t, s)


def set_silent_output_rst2man(env):
    t = 'RST2MAN'
    s = make_silent_output_str(env, t)
    set_silent_output_str(env, t, s)


def set_silent_output_rst2html(env):
    t = 'RST2HTML'
    s = make_silent_output_str(env, t)
    set_silent_output_str(env, t, s)


def set_silent_output_str(env, t, s=None, com=True):
    if s is None:
        s = make_silent_output_str(env, t)
    if com:
        key = t + 'COMSTR'
    else:
        key = t + 'STR'
    env[key] = s


def make_silent_output_str(env, label, src=True, tgt=True, sep=' --> '):
    label = '{:<10}'.format(label)
    if src:
        src = '${SOURCES.file}'
    if tgt:
        tgt = '${TARGETS.file}'

    if src and tgt:
        s = label + ' ' + src + sep + tgt
    elif src:
        s = label + ' ' + src
    elif tgt:
        s = label + ' ' + tgt
    else:
        s = label
    return '  ' + s


# . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .
#
# GCC Flags:
#
GCC_CFLAGS = \
    '''
    -Wnonnull -Wbad-function-cast -Wmissing-prototypes
    -Waggregate-return -Wdeclaration-after-statement -Wnested-externs
    -Wstrict-prototypes -Wold-style-definition
    '''
GCC_CFLAGS_EXTRA = \
    '''
    -Wjump-misses-init -Wunsuffixed-float-constants -Wold-style-declaration
    -std=gnu11
    '''
GCC_CCFLAGS = \
    '''
    -pedantic -Wall -Wextra -Winit-self -Wunused -Winline -Wshadow -Wfloat-equal
    -Wwrite-strings -Wpointer-arith -Wcast-align -Wsign-compare
    -Wredundant-decls -Wformat=2 -Wmissing-include-dirs -Wmissing-declarations
    -Wswitch -Wswitch-enum -Wswitch-default -Wcomment -Wparentheses
    -Wsequence-point -Wpointer-arith -Wdisabled-optimization -Wmain -Wundef
    -Wunknown-pragmas -Wunused-macros -Wendif-labels -Wstrict-aliasing=2
    -fPIC -fwrapv -fstrict-aliasing
    '''  # -Wfatal-errors -Wpadded
GCC_CCFLAGS_EXTRA = \
    '''
    -Wvla -Waddress -Woverlength-strings  -Werror -Wconversion
    -Wsign-conversion -Wunreachable-code -Wwrite-strings
    -Wlarger-than=8192 -Wframe-larger-than=4096 -Wstack-usage=4096
    -Wmissing-field-initializers -Wlogical-op
    -fvisibility=default
    '''
GCC_CXXFLAGS = \
    '''
    -std=c++0x -Wenum-compare -Wsign-promo -Wreorder
    -Woverloaded-virtual -Wnon-virtual-dtor
    '''  # -Wold-style-cast
GCC_DEBUG_FLAGS = \
    '''-DDEBUG=%d -g -ggdb -fno-omit-frame-pointer'''
GCC_SECURITY_FLAGS = \
    '''-fstack-protector -fstack-check'''
GCC_OPTLEVEL_FLAGS = \
    {'0': '-O0', '1': '-O1', '2': '-O2 -D_FORTIFY_SOURCE=2', '3': '-O3'}
GCC_OPTLEVEL0_FLAGS = \
    '''-Wunsafe-loop-optimizations -funsafe-loop-optimizations'''
GCC_LINKFLAGS = \
    '''  '''  # -rdynamic
GCC_GCOV_CCFLAGS = '''--coverage'''
GCC_GCOV_LINKFLAGS = '''--coverage'''
GCC_GCOV_LIBS = '''gcov'''

# . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .
#
# Clang Flags:
#
CLANG_CFLAGS = GCC_CFLAGS + \
    '''
    -Wfatal-errors -fcolor-diagnostics
    -Wshift-overflow -Wliteral-conversion
    '''  # -Wpadded
CLANG_CFLAGS_EXTRA = ' '
CLANG_CCFLAGS = GCC_CCFLAGS
CLANG_CCFLAGS_EXTRA = '''-Wno-format-nonliteral -Wmissing-variable-declarations'''
CLANG_CXXFLAGS =  '''-std=c++0x'''
CLANG_DEBUG_FLAGS = GCC_DEBUG_FLAGS
CLANG_SECURITY_FLAGS = '''-fstack-protector'''
CLANG_OPTLEVEL_FLAGS = GCC_OPTLEVEL_FLAGS
CLANG_LINKFLAGS = GCC_LINKFLAGS
