PyInventor
==========

http://thehubbit.github.io/PyInventor

[![Build Status](https://travis-ci.org/TheHubbit/PyInventor.svg?branch=master)](https://travis-ci.org/TheHubbit/PyInventor)

PyInventor provides a Python binding for the Open Inventor 3D graphics toolkit. The main drivers for this project are:
- Enable the creation, modification and viewing of scene graphs with Python
- Offer a flexible yet easy to use "Python-like" interface to Open Inventor
- Support of Open Inventor extensions without introducing compile time dependencies
- Platform independent and compatible with all major implementations of Open Inventor  (SGI, VSG and Coin)
- Undemanding maintenance of the library
- Compatible with Python 3 (Python 2.x is unsupported)

## Using the Module
This module provides capabilities to work with Inventor scene objects from Python. For an overview on how to use it please visit wiki where you can find a getting started guide and examples (see https://github.com/TheHubbit/PyInventor/wiki). 

## License
PyInventor is Open Source Software and distributed under the BSD 3-Clause License. For full terms see the included COPYING file.

## Installation
The binding can be compiled and installed using included distutils setup script. To compile the module CPython 3.3, numpy and Open Inventor need to be installed on the target system. Before running the setup, some variables that point to the Inventor include and library paths as well as the library names need to be adjusted.
```
# Open Inventor paths and libraries:
oivincpath = ''
oivlibpath = ''
oivlibs = []
```

Calling the following command will first compile PyInventor and then copy the binaries into the site-packages directory.
```
python setup.py install
```
