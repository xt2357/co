#!/usr/bin/env python
# coding=utf8
import subprocess
import sys
import os


def enum(*sequential, **named):
    enums = dict(zip(sequential, range(len(sequential))), **named)
    return type('Enum', (), enums)

COLORS = enum (
    HEADER = '\033[95m',
    OKBLUE = '\033[94m',
    OKGREEN = '\033[92m',
    WARNING = '\033[93m',
    FAIL = '\033[91m',
    ENDC = '\033[0m',
    BOLD = '\033[1m',
    UNDERLINE = '\033[4m',
)

def call(cmd, print_cmd=False):
    if print_cmd:
        print(cmd)
    cmd = cmd.replace(COLORS.HEADER, '')
    cmd = cmd.replace(COLORS.OKBLUE, '')
    cmd = cmd.replace(COLORS.OKGREEN, '')
    cmd = cmd.replace(COLORS.WARNING, '')
    cmd = cmd.replace(COLORS.FAIL, '')
    cmd = cmd.replace(COLORS.ENDC, '')
    cmd = cmd.replace(COLORS.BOLD, '')
    cmd = cmd.replace(COLORS.UNDERLINE, '')
    if subprocess.call(cmd, shell=True, stderr=sys.stderr):
        sys.exit(1)

def compile(filename, to_path=None, tags=[]):
    suffix = '.' + '.'.join(filename.split('.')[-1:])
    filename = '.'.join(filename.split('.')[:-1])
    if not to_path:
        to_path = os.path.dirname(filename)
    to_path = os.path.join(to_path, os.path.basename(filename))
    cmd = 'gcc ' + COLORS.UNDERLINE + COLORS.BOLD + '%s%s'%(filename, suffix) + COLORS.ENDC + ' -o %s.o %s'%(to_path, ' '.join(tags))
    print(COLORS.OKBLUE + '[COMPILE]' + COLORS.ENDC + ' -> %s'%(cmd))
    call(cmd)
    return to_path + '.o'

def link(objects, output_filename, tags=[]):
    cmd = 'gcc '
    for obj in objects:
        cmd += COLORS.UNDERLINE + COLORS.BOLD + obj + COLORS.ENDC + ' '
    cmd += '-o %s'%output_filename
    cmd += ' %s' % (' '.join(tags))
    print(COLORS.OKGREEN + '[LINKING]' + COLORS.ENDC + ' -> %s'%(cmd))
    call(cmd)

def lib_static(objects, output_filename):
    cmd = 'ar rcs %s '%output_filename
    for obj in objects:
        cmd += obj + ' '
    print(COLORS.OKGREEN + '[ARCHIVE]' + COLORS.ENDC + ' -> %s'%(cmd))
    call(cmd)

def make_debug():
    objs = []
    for root, dirs, files in os.walk('src'):
        for f in [i for i in files if i.endswith('.cpp')]:
            objs.append(compile(os.path.join(root, f), 'objects', ['-Wall', '-g', '-c', '-lstdc++', '-std=c++11']))
    link(objs, 'a.out', ['-ldl', '-lstdc++', '-std=c++11'])

def make_static():
    objs = []
    for root, dirs, files in os.walk('src'):
        for f in [i for i in files if i.endswith('.cpp') and i != 'main.cpp']:
            objs.append(compile(os.path.join(root, f), 'objects', ['-Wall', '-g', '-c', '-lstdc++', '-std=c++11']))
    lib_static(objs, 'lib/libco.a')

def make_dynamic():
    objs = []
    for root, dirs, files in os.walk('src'):
        for f in [i for i in files if i.endswith('.cpp') and i != 'main.cpp']:
            objs.append(compile(os.path.join(root, f), 'objects', ['-fPIC', '-Wall', '-g', '-c', '-lstdc++', '-std=c++11']))
    link(objs, 'lib/libco.so', ['-shared', '-ldl', '-lstdc++', '-std=c++11'])

def make_test():
    make_dynamic()
    objs = []
    for root, dirs, files in os.walk('test'):
        for f in [i for i in files if i.endswith('.cpp')]:
            objs.append(compile(os.path.join(root, f), 'objects', ['-Wall', '-g', '-c', '-lstdc++', '-std=c++11']))
    link(objs, 'test.out', ['-L./lib', '-lco', '-lstdc++', '-std=c++11', '-Wl,-rpath=./lib'])

def make_clean():
    call('rm -f objects/*.o', print_cmd=True)
    call('rm -f lib/*.so', print_cmd=True)
    call('rm -f *.out', print_cmd=True)
    call('rm -f lib/*.a', print_cmd=True)

if __name__=='__main__':
    if len(sys.argv) < 2:
        make_dynamic()
        sys.exit(0)
    if 'clean' == sys.argv[1]:
        make_clean()
    elif 'static' == sys.argv[1]:
        make_static()
    elif 'dynamic' == sys.argv[1]:
        make_dynamic()
    elif 'test' == sys.argv[1]:
        make_test()    
