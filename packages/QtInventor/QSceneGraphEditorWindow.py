# QSceneGraphEditorWindow class implementation
# Author: Thomas Moeller
#
# Copyright (C) the PyInventor contributors. All rights reserved.
# This file is part of PyInventor, distributed under the BSD 3-Clause
# License. For full terms see the included COPYING file.
#


import sys
import inventor as iv
from PySide import QtCore, QtGui, QtOpenGL
from PyInventor.QtInventor import QIVWidget,QInspectorWidget


class QSceneGraphEditorWindow(QtGui.QWidget):
    """
    Main application window class for a scene graph editor. This class
    creates a render viewport to visualize the scene (QIVWidget) and
    an inspector tool (QInspectorWidget) for analyzing or editing a
    scene graph.
    """

    def __init__(self, parent=None):
        """Initializes widgets of scene graph editor"""
        QtGui.QWidget.__init__(self, parent)
        
        self._root = iv.Separator()
        self._filePath = ""
        self.inspectorWidget = QInspectorWidget()
        self.previewWidget = QIVWidget(format=QtOpenGL.QGLFormat(QtOpenGL.QGL.SampleBuffers))
        self.previewWidget.sceneManager.background = (0.3, 0.3, 0.3)

        mainLayout = QtGui.QVBoxLayout()
        mainLayout.setContentsMargins(2, 2, 0, 0)
        mainLayout.setSpacing(0)

        self._horiSplitter = QtGui.QSplitter(QtCore.Qt.Horizontal)
        self._horiSplitter.addWidget(self.inspectorWidget)
        self._horiSplitter.addWidget(self.previewWidget)
        
        self._vertSplitter = QtGui.QSplitter(QtCore.Qt.Vertical)
        self._vertSplitter.addWidget(self._horiSplitter)

        mainLayout.addWidget(self._vertSplitter)

        self.setLayout(mainLayout)
        self.setWindowTitle(self.applicationTitle())
        
        self.inspectorWidget.attach(self.previewWidget.sceneManager.scene)

        # timer for inventor queue processing (delay, timer and idle queues)
        self.idleTimer = QtCore.QTimer()
        self.idleTimer.timeout.connect(iv.process_queues)
        self.idleTimer.start()


    def applicationTitle(self):
        """Returns the default application title"""
        title = "Scene Graph Editor" 
        if len(self._filePath) > 0:
            title = self._filePath.split('/')[-1]
        return title

    
    def load(self, file):
        """Load a scene from file or string"""
        # keep root node instance and copy children instead so root variable in console stays valid
        del self._root[:]
        self._root += iv.read(file)[:]
        if self._root is not None:
            if file[0] is not '#':
                self._filePath = file
            else:
                self._filePath = ""

            self.parent().setWindowTitle(self.applicationTitle())
            viewRoot = iv.Separator()
            self.previewWidget.sceneManager.scene = viewRoot

            if (len(iv.search(self._root, type="DirectionalLight")) == 0):
                viewRoot += iv.DirectionalLight()
            if (len(iv.search(self._root, type="Camera")) == 0):
                viewRoot += [ iv.OrthographicCamera(), self._root ]
                self.previewWidget.sceneManager.view_all()
            else:
                viewRoot += self._root

            self.inspectorWidget.attach(self._root)


    def save(self):
        """Save current scene to file"""
        if len(self._filePath):
            iv.write(self._root, self._filePath)
        else:
            self.saveAs()

    def saveAs(self):
        """Save current scene to new file"""
        self._filePath = QtGui.QFileDialog.getSaveFileName(self, 'Save', self._filePath, filter="Open Inventor (*.iv);;VRML (*.wrl)")[0]
        if len(self._filePath):
            iv.write(self._root, self._filePath)
            self.parent().setWindowTitle(self.applicationTitle())

    def open(self):
        """Open scene from file"""
        filePath = QtGui.QFileDialog.getOpenFileName(self, 'Open', self._filePath, filter="Scene Graph (*.iv;*.wrl);;Autodesk 3D Studio (*.3ds);;STereoLithography (*.stl)")[0]
        if len(filePath):
            self.load(filePath)

    def new(self):
        """Reset to default scene"""
        if QtGui.QMessageBox.warning(None, "New Scene Graph",
                "This operation will discard all changes to the existing scene graph. Would you like to continue?",
                QtGui.QMessageBox.Yes | QtGui.QMessageBox.No | QtGui.QMessageBox.Default) is QtGui.QMessageBox.Yes:
            self.load(
                "#Inventor V2.1 ascii\n\nSeparator { "
                "DirectionalLight {} OrthographicCamera { position 0 0 5 height 5 }"
                "TrackballManip {} Material { diffuseColor 1 0 0 }"
                "Cone {} }")

    def manip(self, on):
        """Set scene camera manipulation mode"""
        self.previewWidget.sceneManager.interaction(on)

    def addWidget(self, orientation, widget):
        if orientation is QtCore.Qt.Vertical:
            self._vertSplitter.addWidget(widget)
        elif orientation is QtCore.Qt.Horizontal:
            self._horiSplitter.addWidget(widget)
