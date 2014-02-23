#!/usr/bin/env python

"""PyInventor 'Hello Cone' example using PySide (Qt v4.x)"""

import sys
import inventor as iv
from PySide import QtCore, QtGui, QtOpenGL

try:
    from OpenGL import GL
except ImportError:
    app = QtGui.QApplication(sys.argv)
    QtGui.QMessageBox.critical(None, "OpenGL", "PyOpenGL must be installed to run this example.",
                               QtGui.QMessageBox.Ok | QtGui.QMessageBox.Default,
                               QtGui.QMessageBox.NoButton)
    sys.exit(1)


class Window(QtGui.QWidget):
    """Main application windw"""
    def __init__(self, parent=None):
        QtGui.QWidget.__init__(self, parent)
        
        self.glWidget = GLWidget()
        mainLayout = QtGui.QHBoxLayout()
        mainLayout.setContentsMargins(0, 0, 0, 0)
        mainLayout.addWidget(self.glWidget)
        self.setLayout(mainLayout)
        self.setWindowTitle(self.tr("Hello Inventor"))


class GLWidget(QtOpenGL.QGLWidget):
    """OpenGL widget for displaying scene"""

    qtButtonIndex = (QtCore.Qt.LeftButton, QtCore.Qt.MiddleButton, QtCore.Qt.RightButton)

    def __init__(self, parent=None):
        QtOpenGL.QGLWidget.__init__(self, parent)
        self.sceneManager = 0
    
    def createScene(self):
        '''Returns a simple scene (light, camera, manip, material, cone)'''
        root =  iv.Separator()
        root += iv.DirectionalLight()
        root += iv.OrthographicCamera()
        root += iv.TrackballManip()
        root += iv.Material("diffuseColor 1 0 0")
        root += iv.Cone()
        return root
    
    def minimumSizeHint(self):
        return QtCore.QSize(100, 100)
    
    def sizeHint(self):
        return QtCore.QSize(512, 512)
    
    def initializeGL(self):
        GL.glEnable(GL.GL_DEPTH_TEST)
        self.sceneManager = iv.SceneManager()
        self.sceneManager.redisplay = self.updateGL
        self.sceneManager.scene = self.createScene()
        self.sceneManager.scene.view_all()
    
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


if __name__ == '__main__':
    app = QtGui.QApplication(sys.argv)
    window = Window()
    window.show()
    sys.exit(app.exec_())

