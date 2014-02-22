from distutils.core import setup, Extension
from distutils.sysconfig import *
import os

# Open Inventor paths and libraries:
oivincpath = '/Library/Frameworks/Inventor.framework/Resources/include'
oivlibpath = '/Library/Frameworks/Inventor.framework/Libraries'
oivlibs = [ 'Coin' ]

# numpy paths are generated automatically:
numarrayinc = os.path.join(get_python_lib(plat_specific=1), 'numpy/numarray/include')
numcoreinc  = os.path.join(get_python_lib(plat_specific=1), 'numpy/core/include')

module1 = Extension('inventor',
                    include_dirs = [numarrayinc, numcoreinc, oivincpath],
                    libraries = oivlibs,
                    library_dirs = [oivlibpath],
                    sources = ['src/PyInventor.cpp', 
                               'src/PySceneManager.cpp', 
                               'src/PySceneObject.cpp', 
                               'src/PySensor.cpp'])

setup (name = 'PyInventor',
       version = '1.0',
       description = 'Python integration for Open Inventor toolkit',
       author = 'Thomas Moeller',
       author_email = '',
       url = '',
       long_description = '''Python integration for Open Inventor toolkit.''',
       ext_modules = [module1])
