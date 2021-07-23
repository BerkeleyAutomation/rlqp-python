import os
import shutil as sh
import sys
from glob import glob
from platform import system
from shutil import copyfile, copy
from subprocess import call, check_output

from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext
import distutils.sysconfig as sysconfig

import argparse

RLQP_ARG_MARK = '--rlqp'

parser = argparse.ArgumentParser(description='RLQP Setup script arguments.')
parser.add_argument(
    RLQP_ARG_MARK,
    dest='rlqp',
    action='store_true',
    default=False,
    help='Put this first to ensure following arguments are parsed correctly')
parser.add_argument(
    '--long',
    dest='long',
    action='store_true',
    default=False,
    help='Use long integers')
parser.add_argument(
    '--debug',
    dest='debug',
    action='store_true',
    default=False,
    help='Compile extension in debug mode')
args, unknown = parser.parse_known_args()

# necessary to remove RLQP args before passing to setup:
if RLQP_ARG_MARK in sys.argv:
    sys.argv = sys.argv[0:sys.argv.index(RLQP_ARG_MARK)]

# Add parameters to cmake_args and define_macros
cmake_args = ["-DUNITTESTS=OFF"]
cmake_build_flags = []
define_macros = []
lib_subdir = []

# Check if windows linux or mac to pass flag
if system() == 'Windows':
    cmake_args += ['-G', 'Visual Studio 14 2015']
    # Differentiate between 32-bit and 64-bit
    if sys.maxsize // 2 ** 32 > 0:
        cmake_args[-1] += ' Win64'
    cmake_build_flags += ['--config', 'Release']
    lib_name = 'rlqp.lib'
    lib_subdir = ['Release']

else:  # Linux or Mac
    cmake_args += ['-G', 'Unix Makefiles']
    lib_name = 'librlqp.a'

# Pass Python option to CMake and Python interface compilation
cmake_args += ['-DPYTHON=ON']

# Remove long integers for numpy compatibility (default args.long == False)
# https://github.com/numpy/numpy/issues/5906
# https://github.com/ContinuumIO/anaconda-issues/issues/3823
if not args.long:
    print("Disabling LONG\n" +
          "Remove long integers for numpy compatibility. See:\n" +
          " - https://github.com/numpy/numpy/issues/5906\n" +
          " - https://github.com/ContinuumIO/anaconda-issues/issues/3823\n" +
          "You can reenable long integers by passing: "
          "--rlqp --long argument.\n")
    cmake_args += ['-DDLONG=OFF']

# Pass python to compiler launched from setup.py
define_macros += [('PYTHON', None)]

# Pass python include dirs to cmake
cmake_args += ['-DPYTHON_INCLUDE_DIRS=%s' % sysconfig.get_python_inc()]

class get_torch_cmake_path(object):
    """Returns torch's cmake path with lazy import.
    """
    def __str__(self):
        import torch
        print("Using Torch_DIR=%s/Torch" % torch.utils.cmake_prefix_path)
        return '-DTorch_DIR=%s/Torch' % torch.utils.cmake_prefix_path


cmake_args += [get_torch_cmake_path()]

# Define rlqp and qdldl directories
current_dir = os.getcwd()
rlqp_dir = os.path.join('rlqp_sources')
rlqp_build_dir = os.path.join(rlqp_dir, 'build')
qdldl_dir = os.path.join(rlqp_dir, 'lin_sys', 'direct', 'qdldl')


# Interface files
class get_numpy_include(object):
    """Returns Numpy's include path with lazy import.
    """
    def __str__(self):
        import numpy
        return numpy.get_include()


include_dirs = [
    os.path.join(rlqp_dir, 'include'),      # osqp.h
    os.path.join(qdldl_dir),                # qdldl_interface header to
                                            # extract workspace for codegen
    os.path.join(qdldl_dir, "qdldl_sources",
                            "include"),     # qdldl includes for file types
    os.path.join('extension', 'include'),   # auxiliary .h files
    get_numpy_include()]                    # numpy header files

sources_files = glob(os.path.join('extension', 'src', '*.c'))

# Set optimizer flag
if system() != 'Windows':
    compile_args = ["-O3"]
else:
    compile_args = []

# If in debug mode
if args.debug:
    print("Debug mode")
    compile_args += ["-g"]
    cmake_args += ["-DCMAKE_BUILD_TYPE=Debug"]
else:
    print("Release mode")
    cmake_args += ["-DCMAKE_BUILD_TYPE=Release"]

# External libraries
library_dirs = []
libraries = []
if system() == 'Linux':
    libraries += ['rt']
if system() == 'Windows':
    # They moved the stdio library to another place.
    # We need to include this to fix the dependency
    libraries += ['legacy_stdio_definitions']

class get_torch_library_dir(object):
    """Returns torch's library path with lazy import.
    """
    def __str__(self):
        import torch
        return os.path.join(os.path.dirname(os.path.dirname(torch.utils.cmake_prefix_path)), "lib")

library_dirs += [get_torch_library_dir()]
libraries += ["torch", "c10"]

# Add RLQP compiled library
extra_objects = [os.path.join('extension', 'src', lib_name)]

'''
Copy C sources for code generation
'''

# Create codegen directory
rlqp_codegen_sources_dir = os.path.join('module', 'codegen', 'sources')
if os.path.exists(rlqp_codegen_sources_dir):
    sh.rmtree(rlqp_codegen_sources_dir)
os.makedirs(rlqp_codegen_sources_dir)

# RLQP C files
cfiles = [os.path.join(rlqp_dir, 'src', f)
          for f in os.listdir(os.path.join(rlqp_dir, 'src'))
          if (f.endswith('.c') or f.endswith('.cpp')) and f not in ('cs.c', 'ctrlc.c', 'polish.c',
                                            'lin_sys.c')]
cfiles += [os.path.join(qdldl_dir, f)
           for f in os.listdir(qdldl_dir)
           if f.endswith('.c')]
cfiles += [os.path.join(qdldl_dir, 'qdldl_sources', 'src', f)
           for f in os.listdir(os.path.join(qdldl_dir, 'qdldl_sources',
                                            'src'))]
rlqp_codegen_sources_c_dir = os.path.join(rlqp_codegen_sources_dir, 'src')
if os.path.exists(rlqp_codegen_sources_c_dir):  # Create destination directory
    sh.rmtree(rlqp_codegen_sources_c_dir)
os.makedirs(rlqp_codegen_sources_c_dir)
for f in cfiles:  # Copy C files
    copy(f, rlqp_codegen_sources_c_dir)

# List with RLQP H files
hfiles = [os.path.join(rlqp_dir, 'include', f)
          for f in os.listdir(os.path.join(rlqp_dir, 'include'))
          if f.endswith('.h') and f not in ('qdldl_types.h',
                                            'osqp_configure.h',
                                            'cs.h', 'ctrlc.h', 'polish.h',
                                            'lin_sys.h')]
hfiles += [os.path.join(qdldl_dir, f)
           for f in os.listdir(qdldl_dir)
           if f.endswith('.h')]
hfiles += [os.path.join(qdldl_dir, 'qdldl_sources', 'include', f)
           for f in os.listdir(os.path.join(qdldl_dir, 'qdldl_sources',
                                            'include'))
           if f.endswith('.h')]
rlqp_codegen_sources_h_dir = os.path.join(rlqp_codegen_sources_dir, 'include')
if os.path.exists(rlqp_codegen_sources_h_dir):  # Create destination directory
    sh.rmtree(rlqp_codegen_sources_h_dir)
os.makedirs(rlqp_codegen_sources_h_dir)
for f in hfiles:  # Copy header files
    copy(f, rlqp_codegen_sources_h_dir)

# List with RLQP configure files
configure_files = [os.path.join(rlqp_dir, 'configure', 'osqp_configure.h.in'),
                   os.path.join(qdldl_dir, 'qdldl_sources', 'configure',
                                'qdldl_types.h.in')]
rlqp_codegen_sources_configure_dir = os.path.join(rlqp_codegen_sources_dir,
                                                  'configure')
if os.path.exists(rlqp_codegen_sources_configure_dir):
    sh.rmtree(rlqp_codegen_sources_configure_dir)
os.makedirs(rlqp_codegen_sources_configure_dir)
for f in configure_files:  # Copy configure files
    copy(f, rlqp_codegen_sources_configure_dir)

# Copy cmake files
copy(os.path.join(rlqp_dir, 'src',     'CMakeLists.txt'),
     rlqp_codegen_sources_c_dir)
copy(os.path.join(rlqp_dir, 'include', 'CMakeLists.txt'),
     rlqp_codegen_sources_h_dir)


class build_ext_rlqp(build_ext):
    def build_extensions(self):
        # Compile RLQP using CMake

        # Create build directory
        if os.path.exists(rlqp_build_dir):
            sh.rmtree(rlqp_build_dir)
        os.makedirs(rlqp_build_dir)
        os.chdir(rlqp_build_dir)

        try:
            check_output(['cmake', '--version'])
        except OSError:
            raise RuntimeError("CMake must be installed to build RLQP")

        # Compile static library with CMake
        call(['cmake'] + [str(x) for x in cmake_args] + ['..'])
        call(['cmake', '--build', '.', '--target', 'rlqpstatic'] +
             cmake_build_flags)

        # Change directory back to the python interface
        os.chdir(current_dir)

        # Copy static library to src folder
        lib_origin = [rlqp_build_dir, 'out'] + lib_subdir + [lib_name]
        lib_origin = os.path.join(*lib_origin)
        copyfile(lib_origin, os.path.join('extension', 'src', lib_name))

        # Run extension
        build_ext.build_extensions(self)


_rlqp = Extension('rlqp._rlqp',
                  define_macros=define_macros,
                  libraries=libraries,
                  library_dirs=library_dirs,
                  include_dirs=include_dirs,
                  extra_objects=extra_objects,
                  sources=sources_files,
                  extra_compile_args=compile_args)

packages = ['rlqp',
            'rlqp.codegen',
            'rlqp.tests',
            'rlqppurepy']


# Read README.rst file
def readme():
    with open('README.rst') as f:
        return f.read()


with open('requirements.txt') as f:
    requirements = f.read().splitlines()

setup(name='rlqp',
      version='0.6.2.post0.alpha0',
      author='Bartolomeo Stellato, Goran Banjac, Jeff Ichnowski, Paras Jain',
      author_email='bartolomeo.stellato@gmail.com',
      description='RLQP: Reinforcement Learning for QP Solving (based on OSQP)',
      long_description=readme(),
      package_dir={'rlqp': 'module',
                   'rlqppurepy': 'modulepurepy'},
      include_package_data=True,  # Include package data from MANIFEST.in
      setup_requires=["numpy >= 1.7", "qdldl"],
      install_requires=requirements,
      license='Apache 2.0',
      url="https://berkeleyautomation.github.io/rlqp",
      cmdclass={'build_ext': build_ext_rlqp},
      packages=packages,
      ext_modules=[_rlqp])
