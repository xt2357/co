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

def compile(filename, to_path=None):
    suffix = '.' + '.'.join(filename.split('.')[-1:])
    filename = '.'.join(filename.split('.')[:-1])
    if not to_path:
        to_path = os.path.dirname(filename)
    to_path = os.path.join(to_path, os.path.basename(filename))
    cmd = 'gcc -Wall -g -c %s%s -o %s.o -lstdc++ -std=c++11'%(filename, suffix, to_path)
    print('[COMPILE]-> %s: %s'%(filename + suffix, cmd))
    call(cmd)
    return to_path + '.o'

def link(objects, output_filename):
    cmd = 'gcc '
    for obj in objects:
        cmd += obj + ' '
    cmd += '-o %s'%output_filename
    print('[LINK]-> %s: %s'%(' '.join(objects), cmd))
    call(cmd)


def make_debug():
    objs = []
    for root, dirs, files in os.walk('src'):
        for f in [i for i in files if i.endswith('.cpp')]:
            objs.append(compile(os.path.join(root, f), 'objects'))
    link(objs, 'a.out')
    
def make_clean():
    call('rm objects/*.o', print_cmd=True)
    call('rm *.out', print_cmd=True)


if __name__=='__main__':
    if len(sys.argv) < 2:
        make_debug()
        sys.exit(0)
    if 'clean' == sys.argv[1]:
        make_clean()

