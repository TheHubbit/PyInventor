# Scene graph editor application implementation
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
    Main application window for scene graph editor.
    """

    def __init__(self, parent=None):
        """Initializes widgets of scene graph editor"""
        QtGui.QWidget.__init__(self, parent)
        
        self._filePath = ""
        self.inspectorWidget = QInspectorWidget()
        self.previewWidget = QIVWidget(format=QtOpenGL.QGLFormat(QtOpenGL.QGL.SampleBuffers))
        self.previewWidget.sceneManager.background = (0.3, 0.3, 0.3)

        mainLayout = QtGui.QHBoxLayout()
        mainLayout.setContentsMargins(2, 2, 0, 0)
        mainLayout.setSpacing(0)

        horiSplitter = QtGui.QSplitter(QtCore.Qt.Horizontal)
        horiSplitter.addWidget(self.inspectorWidget)
        horiSplitter.addWidget(self.previewWidget)
        
        mainLayout.addWidget(horiSplitter)
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
        self._root = iv.read(file)
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
        self._filePath = QtGui.QFileDialog.getSaveFileName(self, 'Save', self._filePath, filter="Open Inventor (*.iv);;Virtual Reality Modeling Language (*.wrl)")[0]
        if len(self._filePath):
            print(self._filePath)
            iv.write(self._root, self._filePath)

    def open(self):
        """Open scene from file"""
        filePath = QtGui.QFileDialog.getOpenFileName(self, 'Open', self._filePath, filter="Open Inventor (*.iv);;Virtual Reality Modeling Language (*.wrl)")[0]
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


class QSceneGraphEditor(QtGui.QApplication):
    """
    Scene graph editor application.
    """

    def __init__(self, args):
        """Initializes application"""
        super(QSceneGraphEditor, self).__init__(args)

        self._mainWindow = QtGui.QMainWindow()
        self._editor = QSceneGraphEditorWindow()
        self._mainWindow.setCentralWidget(self._editor)
    
        exitAction = QtGui.QAction('&Exit', self._mainWindow)
        exitAction.setShortcut('Ctrl+Q')
        exitAction.setStatusTip('Exit application')
        exitAction.triggered.connect(self._mainWindow.close)

        newAction = QtGui.QAction('&New', self._mainWindow)
        newAction.setShortcut('Ctrl+N')
        newAction.setStatusTip('Creates a simple default scene')
        newAction.triggered.connect(self._editor.new)

        openAction = QtGui.QAction('&Open...', self._mainWindow)
        openAction.setShortcut('Ctrl+O')
        openAction.setStatusTip('Open scene graph from file')
        openAction.triggered.connect(self._editor.open)

        saveAction = QtGui.QAction('&Save', self._mainWindow)
        saveAction.setShortcut('Ctrl+S')
        saveAction.setStatusTip('Save scene graph')
        saveAction.triggered.connect(self._editor.save)

        saveAsAction = QtGui.QAction('&Save As...', self._mainWindow)
        saveAsAction.setStatusTip('Save scene graph to another file')
        saveAsAction.triggered.connect(self._editor.saveAs)

        insertObjectAction = QtGui.QAction('&Insert...', self._mainWindow)
        insertObjectAction.setShortcut('Ctrl+I')
        insertObjectAction.setStatusTip('Inserts new scene object before current one')
        insertObjectAction.triggered.connect(self._editor.inspectorWidget.insertObject)

        appendObjectAction = QtGui.QAction('&Append...', self._mainWindow)
        appendObjectAction.setShortcut('Ctrl+K')
        appendObjectAction.setStatusTip('Inserts new scene object after current one')
        appendObjectAction.triggered.connect(self._editor.inspectorWidget.appendObject)

        deleteObjectAction = QtGui.QAction('&Delete', self._mainWindow)
        deleteObjectAction.setShortcut('DEL')
        deleteObjectAction.setStatusTip('Deletes currently selected scene object')
        deleteObjectAction.triggered.connect(self._editor.inspectorWidget.deleteObject)

        viewAllAction = QtGui.QAction('View &All', self._mainWindow)
        viewAllAction.setShortcut('Ctrl+A')
        viewAllAction.setStatusTip('Resets camera in scene so all object are visible')
        viewAllAction.triggered.connect(self._editor.previewWidget.sceneManager.view_all)

        viewManipAction = QtGui.QAction('Camera &Manipulation', self._mainWindow)
        viewManipAction.setShortcut('Ctrl+M')
        viewManipAction.setStatusTip('Enables manipulation of camera in the scene')
        viewManipAction.setCheckable(True)
        viewManipAction.setChecked(True)
        viewManipAction.connect(QtCore.SIGNAL("triggered(bool)"), self._editor.previewWidget.sceneManager.interaction)
        self._editor.previewWidget.sceneManager.interaction(True)

        #self._mainWindow.statusBar()

        menubar = self._mainWindow.menuBar()
        fileMenu = menubar.addMenu('&File')
        fileMenu.addAction(newAction)
        fileMenu.addAction(openAction)
        fileMenu.addAction(saveAction)
        fileMenu.addAction(saveAsAction)
        fileMenu.addSeparator()
        fileMenu.addAction(exitAction)
    
        objectsMenu = menubar.addMenu('&Scene Object')
        objectsMenu.addAction(insertObjectAction)
        objectsMenu.addAction(appendObjectAction)
        objectsMenu.addAction(deleteObjectAction)
    
        viewMenu = menubar.addMenu('&View')
        viewMenu.addAction(viewAllAction)
        viewMenu.addAction(viewManipAction)


        if (len(self.arguments()) > 1) and ("." in self.arguments()[-1]):
            # load scene from file if argument is given
            self._editor.load(self.arguments()[-1])
        else:
            self._editor.load(
                "#Inventor V2.1 ascii\n\nSeparator { "
                "DirectionalLight {} OrthographicCamera { position 0 0 5 height 5 }"
                "TrackballManip {} Material { diffuseColor 1 0 0 }"
                "Cone {} }")

        self._mainWindow.show()


if __name__ == '__main__':
    app = QSceneGraphEditor(sys.argv)
    sys.exit(app.exec_())

