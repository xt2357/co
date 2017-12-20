#!/usr/bin/env python
# coding=utf8
import subprocess
import sys
import os

def call(cmd, print_cmd=False):
    if print_cmd:
        print(cmd)
    if subprocess.call(cmd, shell=True, stderr=sys.stderr):
        sys.exit(1)

def compile(filename, to_path=None, tags=[]):
    suffix = '.' + '.'.join(filename.split('.')[-1:])
    filename = '.'.join(filename.split('.')[:-1])
    if not to_path:
        to_path = os.path.dirname(filename)
    to_path = os.path.join(to_path, os.path.basename(filename))
    cmd = 'gcc %s%s -o %s.o %s'%(filename, suffix, to_path, ' '.join(tags))
    print('[COMPILE]-> %s: %s'%(filename + suffix, cmd))
    call(cmd)
    return to_path + '.o'

def link(objects, output_filename, tags=[]):
    cmd = 'gcc '
    for obj in objects:
        cmd += obj + ' '
    cmd += '-o %s'%output_filename
    cmd += ' %s' % (' '.join(tags))
    print('[LINK]-> %s: %s'%(' '.join(objects), cmd))
    call(cmd)

def lib_static(objects, output_filename):
    cmd = 'ar rcs %s '%output_filename
    for obj in objects:
        cmd += obj + ' '
    print('[LIB_STATIC]-> %s: %s'%(' '.join(objects), cmd))
    call(cmd)

def lib_dynamic(objects, output_filename):
    cmd = 'gcc -shared -o %s '%output_filename
    for obj in objects:
        cmd += obj + ' '
    print('[LIB_DYNAMIC]-> %s: %s'%(' '.join(objects), cmd))
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
    lib_dynamic(objs, 'lib/libco.so')
    
def make_clean():
    call('rm -f objects/*.o', print_cmd=True)
    call('rm -f lib/*.so', print_cmd=True)
    call('rm -f *.out', print_cmd=True)
    call('rm -f lib/*.a', print_cmd=True)

def make_test():
    make_dynamic()
    objs = []
    for root, dirs, files in os.walk('test'):
        for f in [i for i in files if i.endswith('.cpp')]:
            objs.append(compile(os.path.join(root, f), 'objects', ['-Wall', '-g', '-c', '-lstdc++', '-std=c++11']))
    link(objs, 'test.out', ['-L./lib', '-lco', '-ldl', '-lstdc++', '-std=c++11', '-Wl,-rpath=./lib'])
    # gcc XXX.c -o xxx.out -L$HOME/.usr/lib -lXX -Wl,-rpath=/home/user/.usr/lib


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
