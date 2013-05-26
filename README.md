PyInventor
==========

PyInventor provides a Python binding for the Open Inventor 3D graphics toolkit. The main drivers for this project are:
- Enable the creation, modification and viewing of scene graphs with Python
- Offer a flexible yet easy to use "Python-like" interface to Open Inventor
- Support of Open Inventor extensions without introducing compile time dependencies
- Platform independent and compatible with all major implementations of Open Inventor  (SGI, VSG and Coin)
- Undemanding maintenance of the library
- Compatible with Python 3

## Building
The binding can be compiled and installed using included distutils setup script. To compile the module CPython 3.3, numpy and Open Inventor need to be installed on the target system. Before running the setup, some variables that point to the Inventor include and library paths as well as the library names need to be adjusted.
```
# Open Inventor paths and libraries:
oivincpath = ''
oivlibpath = ''
oivlibs = []
```

Afterwards calling the following command will first compile PyInventor and then copy the binaries into the site-packages directory.
```
python setup.py install
```

## Using the Module
This module provides capabilities to work with Inventor scene objects from Python. For a detailed description on how to use this module please visit https://github.com/TheHubbit/PyInventor/wiki/Using-PyInventor.

## License
PyInventor is distributed under the BSD 3-Clause License. For full terms see the included COPYING file.

## Links
- SGI Open Inventor (open source): http://oss.sgi.com/projects/inventor/
- VSG Open Inventor (commercial implementation) developer zone: http://oivdoc92.vsg3d.com/
- Open Inventor extensions for medical imaging: https://collab01a.scr.siemens.com/xipwiki/index.php/Main_Page

