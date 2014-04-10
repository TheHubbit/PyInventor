# QIVWidget class implementation
# Author: Thomas Moeller
#
# Copyright (C) the PyInventor contributors. All rights reserved.
# This file is part of PyInventor, distributed under the BSD 3-Clause
# License. For full terms see the included COPYING file.
#

from PySide import QtCore, QtGui, QtOpenGL
from OpenGL import GL
import inventor

class QIVWidget(QtOpenGL.QGLWidget):
    """OpenGL widget for displaying inventor scene graphs"""
    
    # used to map Qt buttons to simple index
    qtButtonIndex = (QtCore.Qt.LeftButton, QtCore.Qt.MiddleButton, QtCore.Qt.RightButton)

    # queue processing timer on class (not instance) level
    idleTimer = QtCore.QTimer()
    idleTimer.timeout.connect(inventor.process_queues)

    def __init__(self, parent=None):
        QtOpenGL.QGLWidget.__init__(self, parent)
        self.sizeHintValue = QtCore.QSize(512, 512)
        self.sceneManager = inventor.SceneManager()
        self.sceneManager.redisplay = self.updateGL
        # timer must be started from QThread
        self.idleTimer.start()

    def initializeGL(self):
        GL.glEnable(GL.GL_DEPTH_TEST)
    
    def paintGL(self):
        self.sceneManager.render()
    
    def resizeGL(self, width, height):
        self.sceneManager.resize(width, height)
    
    def mousePressEvent(self, event):
        self.sceneManager.mouse_button(self.qtButtonIndex.index(event.button()), 0, event.x(), event.y())
    
    def mouseReleaseEvent(self, event):
        self.sceneManager.mouse_button(self.qtButtonIndex.index(event.button()), 1, event.x(), event.y())
    
    def mouseMoveEvent(self, event):
        self.sceneManager.mouse_move(event.x(), event.y())

    def sizeHint(self):
        return self.sizeHintValue

    def setSizeHint(self, size):
        self.sizeHintValue = QtCore.QSize(size)

    def setSizeHint(self, width, height):
        self.sizeHintValue = QtCore.QSize(width, height)

