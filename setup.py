from distutils.core import setup, Extension
from distutils.sysconfig import *
import os

# Open Inventor paths and libraries:

# OS X paths by default
oivincpath = '/Library/Frameworks/Inventor.framework/Resources/include'
oivlibpath = '/Library/Frameworks/Inventor.framework/Libraries'
oivlibname = 'Coin'

# Check for "--static" option (Coin only)
if "--static" in sys.argv:
    sys.argv.remove("--static")
    libsuffix = "s"
    oivcompileargs = ['-DCOIN_NOT_DLL']
    oivlibs = ['simage1s']
else:
    libsuffix = ""
    oivcompileargs = ['-DCOIN_DLL']
    oivlibs = [ ]

# Coin environment variable?
if os.environ.get('COINDIR') is not None:
    oivincpath = os.environ.get('COINDIR') + '/include'
    oivlibpath = os.environ.get('COINDIR') + '/lib'
    if os.environ.get('windir') is not None:
        # Coin on Windows uses Coin4 rather than just Coin as library name
        oivlibname = oivlibname + '4'

# Additional libs
oivlibs += [ oivlibname + libsuffix ]
if os.environ.get('windir') is not None:
    oivlibs += [ 'Opengl32', 'Gdi32', 'User32', 'Gdiplus' ]


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
                               'src/PySensor.cpp'])

setup (name = 'PyInventor',
       version = '1.0',
       description = 'Python integration for Open Inventor toolkit',
       author = 'Thomas Moeller',
       author_email = '',
       url = 'http://thehubbit.github.io/PyInventor/',
       long_description = '''Python integration for Open Inventor toolkit.''',
       ext_modules = [module1],
       package_dir={'PyInventor': 'packages'},
       packages=['PyInventor', 'PyInventor.QtInventor'])
