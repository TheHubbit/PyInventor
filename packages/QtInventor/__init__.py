"""
Helper classes for creating 3D applications with PySide.

PySide is a binding to the Qt cross-platform application framework. This
package contains helper classes that integrate Open Inventor / Coin3D
into PySide based applications, namely:
- QIVWidget: Viewport widget for rendering and interacting with scene
  graphs.
- QInspectorWidget: Scene graph inspector showing the scene structure in
  a tree view and the fields of a node in a table view.
"""

from .QIVWidget import *
from .QInspectorWidget import *
from .QSceneGraphEditorWindow import *
from .QSceneGraphEditor import *
