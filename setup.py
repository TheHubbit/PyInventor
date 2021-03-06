from setuptools import setup, find_packages
from distutils.core import Extension
from distutils.sysconfig import *
import os

# Open Inventor paths and libraries:

# OS X paths by default
# (CMake branch of Coin installs to /usr/local/... rather than Frameworks)
#oivincpath = '/Library/Frameworks/Inventor.framework/Resources/include'
#oivlibpath = '/Library/Frameworks/Inventor.framework/Libraries'
oivincpath = '/usr/local/include'
oivlibpath = '/usr/local/lib'
oivlibname = 'Coin'

oivlibs = []
oivcompileargs = []
libsuffix = ''

# Check for '--debug' option
if '--debug' in sys.argv:
    sys.argv.remove('--debug')
    if os.environ.get('windir') is not None:
        libsuffix += 'd'
        oivcompileargs.append('/MDd')

# Check for '--static' option
if '--static' in sys.argv:
    sys.argv.remove('--static')
    libsuffix = 's' + libsuffix
    oivcompileargs.append('-DCOIN_NOT_DLL')
    if os.environ.get('windir') is not None:
        oivlibs.append('simage1' + libsuffix)
else:
    oivcompileargs.append('-DCOIN_DLL')

# Coin environment variable?
if os.environ.get('COINDIR') is not None:
    oivincpath = os.environ.get('COINDIR') + '/include'
    oivlibpath = os.environ.get('COINDIR') + '/lib'

# Paths in command line options?
if "--incpath" in sys.argv:
    idx = sys.argv.index("--incpath")
    oivincpath = sys.argv[idx + 1]
    del sys.argv[idx : idx + 2]
if "--libpath" in sys.argv:
    idx = sys.argv.index("--libpath")
    oivlibpath = sys.argv[idx + 1]
    del sys.argv[idx : idx + 2]

# Additional libs
if os.environ.get('windir') is not None:
    oivlibs += ['Opengl32', 'Gdi32', 'User32', 'Gdiplus']
else:
    oivlibs += [oivlibname + libsuffix]


# numpy paths are generated automatically:
numarrayinc = os.path.join(get_python_lib(plat_specific=1), 'numpy/numarray/include')
numcoreinc  = os.path.join(get_python_lib(plat_specific=1), 'numpy/core/include')

module1 = Extension('inventor',
                    include_dirs = [numarrayinc, numcoreinc, oivincpath],
                    libraries = oivlibs,
                    library_dirs = [oivlibpath],
                    extra_compile_args = oivcompileargs,
                    sources = ['src/PyInventor.cpp', 
                               'src/PySceneManager.cpp', 
                               'src/PySceneObject.cpp', 
                               'src/PySensor.cpp',
							   'src/PyField.cpp',
                               'src/PyEngineOutput.cpp',
                               'src/PyNodekitCatalog.cpp',
                               'src/PyPath.cpp'])

setup (name = 'PyInventor',
       version = '1.2',
       description = 'Python integration for Open Inventor toolkit',
       author = 'Thomas Moeller',
       author_email = '',
       url = 'http://thehubbit.github.io/PyInventor/',
       long_description = '''Python integration for Open Inventor toolkit.''',
       ext_modules = [module1],
       package_dir={'PyInventor': 'packages'},
       packages=['PyInventor', 'PyInventor.QtInventor'],
       setup_requires=['numpy'],
       install_requires=['numpy'])
