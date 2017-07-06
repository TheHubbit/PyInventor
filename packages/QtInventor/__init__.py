"""
Helper classes for creating 3D applications with PySide.

PySide is a binding to the Qt cross-platform application framework. This
package contains helper classes that integrate Open Inventor / Coin3D
into PySide based applications, namely:
- QIVWidget: Viewport widget for rendering and interacting with scene
  graphs.
- QInspectorWidget: Scene graph inspector showing the scene structure in
  a tree view and the fields of a node in a table view.

Extension libraries that provide additional scene objects can be loaded
by defining the IV_LIBS environment variable, which can contain names of
dynamically loadable libraries separated by a semicolon. Those libraries
need to define a function called initAllClasses() that register all scene
objects in the global database.
"""

from .QIVWidget import *
from .QInspectorWidget import *
from .QSceneGraphEditorWindow import *
from .QSceneGraphEditor import *

# load inventor extensions specified in IV_LIBS environment variable
import os
if os.environ.get('IV_LIBS') is not None:
    import ctypes
    import inventor
    for lib in os.environ.get('IV_LIBS').split(";"):
        dll = ctypes.cdll.LoadLibrary(lib)
        if hasattr(dll, 'initAllClasses'):
            dll.initAllClasses()
    inventor.create_classes()
