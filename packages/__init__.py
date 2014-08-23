"""
Python binding for the Open Inventor 3D graphics toolkit.

The intention of this module is to provide Python programmers tools for
easy creation of interactive 3D applications. It interfaces to the Coin3D
or Open Inventor graphics toolkits, which implement a scene graph API on
top of OpenGL. PyInventor consists of two submodules:
- inventor: Low level interface module to the C++ Coin3D or Open Inventor
  library.
- QtInventor: Helper classes for creating PySide applications.

Furthermore the module contains a simple scene graph editor application,
which can be started from the command line with "python -m PyInventor".
"""
