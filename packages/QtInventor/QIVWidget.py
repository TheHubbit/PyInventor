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

    def __init__(self, parent=None, shareWidget=None):
        QtOpenGL.QGLWidget.__init__(self, parent, shareWidget)
        self.sizeHintValue = QtCore.QSize(512, 512)
        self.sceneManager = inventor.SceneManager()
        self.sceneManager.redisplay = self.updateGL
        self.resizeGLWidth = 512
        self.resizeGLHeight = 512
        # timer must be started from QThread
        self.idleTimer.start()

    def initializeGL(self):
        """Performs initial OpenGL setup"""
        GL.glEnable(GL.GL_DEPTH_TEST)
    
    def paintGL(self):
        """Renders scene"""
        self.sceneManager.render()
    
    def resizeGL(self, width, height):
        """Resizes viewport of scene manager"""
        self.sceneManager.resize(width, height)
        self.resizeGLWidth = width
        self.resizeGLHeight = height
    
    def mousePressEvent(self, event):
        """Forwards mouse button press event to scene for processing"""
        self.sceneManager.mouse_button(self.qtButtonIndex.index(event.button()), 0, event.x(), event.y())
    
    def mouseReleaseEvent(self, event):
        """Forwards mouse button release event to scene for processing"""
        self.sceneManager.mouse_button(self.qtButtonIndex.index(event.button()), 1, event.x(), event.y())
    
    def mouseMoveEvent(self, event):
        """Forwards mouse move event to scene for processing"""
        self.sceneManager.mouse_move(event.x(), event.y())

    def sizeHint(self):
        """Returns hint for default widget size"""
        return self.sizeHintValue

    def setSizeHint(self, size):
        """Sets hint for default widget size"""
        self.sizeHintValue = QtCore.QSize(size)

    def setSizeHint(self, width, height):
        """Sets hint for default widget size"""
        self.sizeHintValue = QtCore.QSize(width, height)

    def toImage(self, width, height):
        """Renders the scene into an offline buffer and returns is as QImage instance"""
        self.makeCurrent()
        fbo = QtOpenGL.QGLFramebufferObject(width, height, QtOpenGL.QGLFramebufferObject.CombinedDepthStencil)
        fbo.bind()
        self.initializeGL()
        self.sceneManager.resize(width, height)
        self.sceneManager.render()
        fbo.release()
        
        # restore original viewport size
        self.sceneManager.resize(self.resizeGLWidth, self.resizeGLHeight)

        return fbo.toImage()


