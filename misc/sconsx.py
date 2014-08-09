#
# sconsx.py -- SCons Extensions
#
# Copyright (C) 2010, 2011, 2012, 2013, 2014, 2015, 2016 -- Syanrete
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
import copy
import getpass
import inspect
import datetime
import subprocess


# . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .
#
# GCC Flags:
#
GCC_CFLAGS = \
    '''
    -Wnonnull -Wbad-function-cast -Wmissing-prototypes
    -Waggregate-return -Wdeclaration-after-statement -Wnested-externs
    -Wstrict-prototypes -Wold-style-definition -Winit-self
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
CLANG_CFLAGS_EXTRA = '-Wparentheses-equality -Wno-language-extension-token'
CLANG_CCFLAGS = GCC_CCFLAGS
CLANG_CCFLAGS_EXTRA = '''-Wno-format-nonliteral -Wmissing-variable-declarations'''
CLANG_CXXFLAGS = '''-std=c++0x'''
CLANG_DEBUG_FLAGS = GCC_DEBUG_FLAGS
CLANG_SECURITY_FLAGS = '''-fstack-protector'''
CLANG_OPTLEVEL_FLAGS = GCC_OPTLEVEL_FLAGS
CLANG_LINKFLAGS = GCC_LINKFLAGS


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


class Metadata:

    '''
    Extra meta-data and useful information associate with environment variable.
    '''

    def __init__(self):
        self.arguments = []
        self.environ = {}
        self.user = ''
        self.timenow = ''
        self.timeiso = ''
        self.timefmt = ''
        self.sysname = ''
        self.sysvers = ''
        self.version = ''
        self.release = '1'
        self.revision = ''
        self.variables = None
        self.builddir = '#'
        self.ccname = ''
        self.cflags = ''
        self.ccflags = ''
        self.cxxflags = ''
        self.linkflags = ''
        self.cclibs = ''

# . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .


class OutputFormatter:

    '''
    Auxiliary class to override various output formats into short (silent)
    output format.
    '''

    def __init__(self, env):
        self.silent = True
        self.env = env

    def issilent(self):
        return self.env.issilent()

    def setup_output(self):
        self.setup_builtin_tools_output()
        self.setup_aspp_output()
        self.setup_install_output()
        self.setup_exec_output()
        self.setup_rst2man_output()
        self.setup_rst2html_output()

    def setup_builtin_tools_output(self):
        builtin_tools = ['AR', 'AS', 'CC', 'CXX', 'LINK',
                         'RANLIB', 'SHCC', 'SHCXX', 'SHLINK']
        for t in builtin_tools:
            self.setup_tool_output(t)

    def setup_tool_output(self, tool, src=False):
        p = '{0}'.format(tool)
        s = self.make_silent_output_str(p, src=src, tgt=True)
        self.setup_silent_output_str(tool, s)

    def setup_aspp_output(self):
        self.setup_silent_output_str('ASPP')

    def setup_install_output(self):
        t = 'INSTALL'
        s = self.make_silent_output_str(t)
        self.setup_silent_output_str(t, s)

    def setup_exec_output(self):
        t = 'EXEC'
        s = self.make_silent_output_str(t, tgt=False)
        self.setup_silent_output_str(t, s)

    def setup_rst2man_output(self):
        t = 'RST2MAN'
        s = self.make_silent_output_str(t)
        self.setup_silent_output_str(t, s)

    def setup_rst2html_output(self):
        t = 'RST2HTML'
        s = self.make_silent_output_str(t)
        self.setup_silent_output_str(t, s)

    def setup_silent_output_str(self, t, s=None, com=True):
        if not self.issilent():
            return
        if s is None:
            s = self.make_silent_output_str(t)
        if com:
            key = t + 'COMSTR'
        else:
            key = t + 'STR'
        self.env[key] = s

    def make_command_outout_str(self, label, prog=None, args=''):
        if not self.issilent():
            return ''
        cmdstr = self.make_silent_output_str(label, True, False, '')
        return cmdstr + ' ' + str(args)

    @staticmethod
    def make_silent_output_str(label, src=True, tgt=True, sep=' --> '):
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


class EnvironmentSettings:

    '''
    Base class to encapsulate various settings and command-line variables,
    which are auto-discovered before any action.
    '''

    def __init__(self):
        # Trivial settings
        self.meta = Metadata()
        self.meta.arguments = copy.copy(ARGUMENTS)
        self.meta.environ = dict(os.environ)
        self.meta.user = getpass.getuser()
        self.meta.timenow = datetime.datetime.now()
        self.meta.timeiso = self.meta.timenow.isoformat()
        self.meta.timefmt = self.meta.timeiso.split('.')[0].rsplit(':', 1)[0]
        self.meta.sysname = str(os.uname()[1])
        self.meta.sysvers = str(os.uname()[0]) + '-' + str(os.uname()[2])
        self.meta.version = self.meta.environ.get('VERSION', '0.0.1').strip()
        self.meta.release = self.meta.environ.get('RELEASE', '1').strip()
        # Complex settings
        self.update_version_by_file()
        self.update_revision_by_git()
        self.resolve_variables()

    def update_version_by_file(self):
        try:
            with open('VERSION', 'r') as f:
                ver = f.readlines()[0]
                self.meta.version = ver.strip()
        except:
            pass

    def update_revision_by_git(self):
        try:
            cmd = 'git rev-parse --short=10 HEAD'
            rev = subprocess.check_output(cmd.split())
            self.meta.revision = rev.strip()
        except subprocess.CalledProcessError:
            pass

    def resolve_variables(self):
        vrs = Variables(None, self.meta.arguments)
        vrs.AddVariables(
            ('USER', 'Username', self.meta.user),
            ('DATETIME', 'Datetime', self.meta.timefmt),
            ('SYSNAME', 'Sysname', self.meta.sysname),
            ('SYSVERS', 'Sysvers', self.meta.sysvers),
            ('VERSION', 'Version', self.meta.version),
            ('RELEASE', 'Release', self.meta.release),
            ('REVISION', 'Revision', self.meta.revision),
            (EnumVariable('CC', 'C Compiler', 'gcc', Split('gcc clang'))),
            (EnumVariable('CXX', 'C++ Compiler', 'g++', Split('g++ clang++'))),
            (EnumVariable('DEBUG', 'Debug', '1', Split('0 1 2 3'))),
            (EnumVariable('OPTLEVEL', 'Optimization', '0', Split('0 1 2 3'))),
            (BoolVariable('SILENT', 'Silent', True)),
            (BoolVariable('COVERAGE', 'Code coverage', False)),
            ('BUILDDIR', 'Build directory', '_build'))
        vrs.FormatVariableHelpText = EnvironmentSettings.format_var_helptext
        self.meta.variables = vrs

    @staticmethod
    def format_var_helptext(env, opt, hlp, default, actual, a):
        return '  {0: <11}: {1}\n'.format(opt, actual)

    def setup_cc_config(self, cc, name, versnum, debug, optlevel, cov):
        if 'gcc' in cc:
            self.setup_gcc_config(name, versnum, debug, optlevel, cov)
        elif 'clang' in cc:
            self.setup_clang_config(name, versnum, debug, optlevel, cov)
        else:
            raise RuntimeError("Unsupported compiler {0}".format(cc))

    def setup_gcc_config(self, name, versnum, debug, optlevel, cov):
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

        self.meta.ccname = name
        self.meta.cflags = c_flags
        self.meta.ccflags = cc_flags
        self.meta.cxxflags = cxx_flags
        self.meta.linkflags = link_flags
        self.meta.cclibs = cc_libs

    def setup_clang_config(self, name, versnum, debug, optlevel, cov):
        c_flags = Split(CLANG_CFLAGS) + Split(CLANG_CFLAGS_EXTRA)
        cc_flags = Split(CLANG_CCFLAGS) + Split(CLANG_CCFLAGS_EXTRA)
        cxx_flags = Split(CLANG_CXXFLAGS)
        link_flags = Split(CLANG_LINKFLAGS)
        cc_flags += Split(CLANG_OPTLEVEL_FLAGS[optlevel])
        cc_libs = []
        if int(debug) > 0:
            cc_flags += Split(CLANG_DEBUG_FLAGS % int(debug))
            cc_flags += Split(CLANG_SECURITY_FLAGS)

        self.meta.ccname = name
        self.meta.cflags = c_flags
        self.meta.ccflags = cc_flags
        self.meta.cxxflags = cxx_flags
        self.meta.linkflags = link_flags
        self.meta.cclibs = cc_libs


# . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .

class EnvironmentBase(EnvironmentSettings,
                      SCons.Script.Environment):

    '''Base class for extended Environment with minimal setup & helpers'''

    def __init__(self, *args, **kwargs):
        EnvironmentSettings.__init__(self)
        SCons.Script.Environment.__init__(self,
                                          variables=self.meta.variables,
                                          ENV=self.meta.environ,
                                          *args, **kwargs)

    def variant_dir_of(self, d):
        bd = self.meta.builddir
        dd = self.source_dir_of(d)
        vd = str(os.path.join(bd, str(dd)))
        return vd

    def source_dir_of(self, d):
        dd = self.Dir(d).srcnode().path
        return str(dd)

    def getxlibs(self, libs=None):
        xlibs = self['LIBS']
        if libs:
            xlibs = libs + xlibs
        return xlibs

    def release_number(self):
        return int(self['RELEASE'])

    def issilent(self):
        return self['SILENT']

    def add_method(self, name, method):
        res = AddMethod(self, method, name)
        self.output_formatter.setup_silent_output_str(name)

    def flatten(self, d):
        return self.Flatten(d)

    def compiler_params(self):
        '''Returns tuple of compiler parameters which affects its settings'''
        name = self.compiler_name()
        versnum = self.compiler_versnum()
        debug = self['DEBUG']
        optlevel = self['OPTLEVEL']
        cov = self['COVERAGE']
        return (name, versnum, debug, optlevel, cov)

    def compiler_versnum(self):
        '''Returns compiler version as decimal number'''
        ccversion = self['CCVERSION']
        v = [int(z) for z in ccversion.split('.')]
        (major, minor) = (tuple(v) + (0, 0))[0:2]
        return ((10 * int(major)) + int(minor))

    def compiler_name(self):
        '''Return compiler's striped name from path'''
        return os.path.basename(self['CC'])

    def setup_builddir_dir(self):
        bdir = '#' + self['BUILDDIR']
        self.VariantDir(variant_dir=bdir, src_dir='#', duplicate=0)
        self.meta.builddir = bdir

    def setup_custom_builder(self, bname, builder):
        (com, comstr, builder, exec_cmd) = builder
        self[com] = exec_cmd
        # env[comstr] = None
        self.Append(BUILDERS={bname: builder})

    def create_builder(self, action_name, exec_cmd, src_suffix='', suffix='',
                       single_source=True, ensure_suffix=True):
        return self.make_builder_entry(action_name, exec_cmd, src_suffix,
                                       suffix, single_source, ensure_suffix)

    @staticmethod
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


# . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .

class Environment(EnvironmentBase):

    '''
    Extended wrapper over SCons' Construction Environment with customizations,
    extra builds and method, and pretty-format output.
    '''

    def __init__(self, *args, **kwargs):
        super(Environment, self).__init__(*args, **kwargs)
        self.output_formatter = OutputFormatter(self)
        self.setup_builddir_dir()
        self.setup_compiler_config_flags()
        self.setup_extended_builders()
        self.output_formatter.setup_output()

    def AddBuilderMethod(self,
                         tag,
                         action_name,
                         method,
                         command,
                         src_suffix='',
                         suffix='',
                         single_source=True,
                         ensure_suffix=True):
        builder = self.create_builder(tag, command, src_suffix,
                                      suffix, single_source, ensure_suffix)
        self.setup_custom_builder(action_name, builder)
        self.add_method(tag, method)
        self.output_formatter.setup_silent_output_str(tag)

    def ShowVars(self):
        print('')
        print(self.meta.variables.GenerateHelpText(self))

    def GetVersion(self):
        version = self['VERSION']
        release = self.release_number()
        revision = self['REVISION'] if release > 0 else '0'
        return '{0}-{1}.{2}'.format(version, release, revision)

    def GetTimestamp(self):
        release = self.release_number()
        return self['DATETIME'] if release > 0 else '0'

    def SubSConscript(self, d, include=False, lib=False):
        res = []
        for dd in self.flatten(d):
            vd = self.variant_dir_of(dd)
            sd = '#' + self.source_dir_of(dd)
            if include:
                self.append_includepath(sd)
            if lib:
                self.AppendLibPath(vd)
            rr = self.SConscript(dirs=sd, variant_dir=vd, duplicate=False)
            res.append(rr)
        return res

    def AppendLibPath(self, d):
        dirs = []
        for dd in self.flatten(d):
            sd = '#' + self.source_dir_of(dd)
            vd = self.variant_dir_of(sd)
            dirs.append(self.Dir(vd))
        return self.AppendUnique(LIBPATH=dirs)

    def AppendIncludePath(self, d):
        dirs = []
        for dd in self.flatten(d):
            inp = '#' + self.source_dir_of(dd)
            dirs.append(self.Dir(inp))
        return self.AppendUnique(CPPPATH=dirs)

    def AppendIncludeLibPath(self, d):
        self.AppendLibPath(d)
        return self.AppendIncludePath(d)

    def UnitTest(self, src, LIBS=None, args=''):
        xlibs = self.getxlibs(LIBS)
        return self.ProgramRun(src, xlibs, args)

    def ProgramRun(self, src, LIBS, args=''):
        tgt = '${SOURCE.filebase}'
        prog = self.Program(target=tgt, source=src, LIBS=LIBS)
        return self.ProgramExec(prog, args)

    def ProgramExec(self, prog, args=''):
        tgt = '${SOURCE.filebase}.out'
        cmd = '$SOURCE ' + str(args) + ' > $TARGET'
        cst = self.output_formatter.make_command_outout_str('RUN', prog, args)
        act = Action('@' + cmd, cmdstr=cst)
        return self.Command(tgt, prog, act)

    def Rst2Html(self, source):
        return [self.ExecuteRst2Html(s) for s in self.flatten(source)]

    def Rst2Man(self, source):
        return [self.ExecuteRst2Man(s) for s in self.flatten(source)]

    def setup_compiler_config_flags(self):
        '''Assign compiler flags, specific-custom per known compiler.'''
        self.setup_compiler_config_meta()
        self.AppendUnique(CFLAGS=self.meta.cflags)
        self.AppendUnique(CCFLAGS=self.meta.ccflags)
        self.AppendUnique(CXXFLAGS=self.meta.cxxflags)
        self.AppendUnique(LINKFLAGS=self.meta.linkflags)
        self.AppendUnique(LIBS=self.meta.cclibs)

    def setup_compiler_config_meta(self):
        '''Assign compiler-specific meta-data'''
        cc = self.compiler_name()
        (name, versnum, debug, optlevel, cov) = self.compiler_params()
        self.setup_cc_config(cc, name, versnum, debug, optlevel, cov)

    def setup_extended_builders(self):
        self.setup_rst2man_builder()
        self.setup_rst2html_builder()
        self.setup_execprog_builder()

    def setup_rst2man_builder(self):
        bname = 'ExecuteRst2Man'
        command = 'rst2man $SOURCE | gzip > $TARGET'
        builder = self.create_builder('RST2MAN', command,
                                      src_suffix='.txt', suffix='.gz')
        self.setup_custom_builder(bname, builder)

    def setup_rst2html_builder(self):
        bname = 'ExecuteRst2Html'
        command = 'rst2html $SOURCE > $TARGET'
        builder = self.create_builder('RST2HTML', command,
                                      src_suffix='.txt', suffix='.html')
        self.setup_custom_builder(bname, builder)

    def setup_execprog_builder(self):
        bname = 'ExecuteProgram'
        command = '$SOURCE > $TARGET'
        builder = self.create_builder('EXEC', command, suffix='.out')
        self.setup_custom_builder(bname, builder)

