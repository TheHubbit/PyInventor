# QSceneGraphEditor class implementation
# Author: Thomas Moeller
#
# Copyright (C) the PyInventor contributors. All rights reserved.
# This file is part of PyInventor, distributed under the BSD 3-Clause
# License. For full terms see the included COPYING file.
#


import sys
import inventor as iv
from PySide import QtCore, QtGui, QtOpenGL
from PyInventor.QtInventor import QIVWidget,QInspectorWidget,QSceneGraphEditorWindow


class QSceneGraphEditor(QtGui.QApplication):
    """
    Application class for a scene graph editor.
    
    This application renders scene graph files and displays the scene strcuture
    in a tree view. It furthermore shows the properties (fields) of the scene
    objects in a table view. The structure as well as the fields can be modified
    and changes saved back to file.
    
    This class creates an instance of QSceneGraphEditorWindow as main window
    and links menu entries to it.
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

        refreshObjectAction = QtGui.QAction('&Refresh', self._mainWindow)
        refreshObjectAction.setShortcut('F5')
        refreshObjectAction.setStatusTip('Recreates tree view from root node')
        refreshObjectAction.triggered.connect(self._editor.refresh)

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
        objectsMenu.addSeparator()
        objectsMenu.addAction(refreshObjectAction)
    
        viewMenu = menubar.addMenu('&View')
        viewMenu.addAction(viewAllAction)
        viewMenu.addAction(viewManipAction)

        if "--ipython" in self.arguments():
            # start with IPython console at bottom of application
            from IPython.qt.console.rich_ipython_widget import RichIPythonWidget
            from IPython.qt.inprocess import QtInProcessKernelManager
            from IPython.lib import guisupport
            import inventor
            
            # Create an in-process kernel
            kernel_manager = QtInProcessKernelManager()
            kernel_manager.start_kernel()
            kernel = kernel_manager.kernel
            kernel.gui = 'qt4'
            kernel.shell.push({'iv': inventor, 'root': self._editor._root, 'view': self._editor.previewWidget.sceneManager })

            kernel_client = kernel_manager.client()
            kernel_client.start_channels()

            def stop():
                kernel_client.stop_channels()
                kernel_manager.shutdown_kernel()
                self.exit()

            control = RichIPythonWidget()
            control.kernel_manager = kernel_manager
            control.kernel_client = kernel_client
            control.exit_requested.connect(stop)

            self.addVerticalWidget(control)

        # load default scene or from file if argument is given
        file = "#Inventor V2.1 ascii\n\nSeparator { " \
               "DirectionalLight {} OrthographicCamera { position 0 0 5 height 5 }" \
               "TrackballManip {} Material { diffuseColor 1 0 0 }" \
               "Cone {} }"
        if (len(self.arguments()) > 1) and ("." in self.arguments()[-1]):
            extension = self.arguments()[-1].split('.')[-1].lower()
            if extension in [ 'iv', 'vrml', '3ds', 'stl' ]:
                file = self.arguments()[-1];

        self._editor.load(file)

    def addHorizontalWidget(self, widget):
        """Adds a widget to the right side of the application."""
        self._editor.addWidget(QtCore.Qt.Horizontal, widget)

    def addVerticalWidget(self, widget):
        """Adds a widget to the bottom of the application."""
        self._editor.addWidget(QtCore.Qt.Vertical, widget)

    def show(self):
        """Makes the main application window visible."""
        self._mainWindow.show()

