#!/usr/bin/env python

"""PyInventor Example: Random Discs

   Draws shapes at different positions with various sizes, colors and orientations."""

import sys
import numpy as np
import inventor as iv
from PySide import QtCore, QtGui, QtOpenGL
from PyInventor import QtInventor


def makeRandomScene(n):
    """Returns a scene with randomly placed discs"""
    root =  iv.Separator()
    root += iv.DirectionalLight()
    root += iv.OrthographicCamera("position 0 0 100 nearDistance 20 focalDistance 100 farDistance 160 height 80")
    root += iv.Complexity("value 1")
    shape = iv.Sphere()

    # color table
    cmap = np.array([
            [0, 0, 1], # blue
            [1, 0, 1], # magenta
            [1, 0, 0], # red
            [1, 1, 0], # yellow
            [0, 1, 0], # green
            [0, 1, 1], # cyan
            ])
    cred   = cmap[:, 0]
    cgreen = cmap[:, 1]
    cblue  = cmap[:, 2]
    cidx   = np.linspace(0, 1, len(cmap))

    # visualize random numbers as discs in space using the random values for position and size
    # discs are oriented towards origin but could also represent other dimensions
    rand = np.random.rand(n, 4)
    for (r1, r2, r3, r4) in rand:
        theta = 2 * np.pi * r1
        phi   = np.pi * r2
        r     = np.sqrt(r3)
        scale = 3 * (0.2 + r4)
        trans = np.array([r * np.sin(theta) * np.cos(phi), r * np.sin(theta) * np.sin(phi), r * np.cos(theta)])
        color = (np.interp(r4, cidx, cred), np.interp(r4, cidx, cgreen), np.interp(r4, cidx, cblue))

        disc = iv.ShapeKit()
        disc.shape = shape
        disc.transform.translation = trans * 40.0
        disc.transform.scaleFactor = (scale, 0.3 * scale, scale)
        disc.transform.rotation = ((0.0, 1.0, 0.0), trans)
        disc.appearance.material.diffuseColor = color
        disc.appearance.material.ambientColor = (0.7, 0.7, 0.7)
        root += disc

    return root

# makeRandomScene()


class QMyPickWidget(QtInventor.QIVWidget):
    """Class that turns picked objects white (for demonstrating pick action)"""
    def mousePressEvent(self, event):
        """Forwards mouse button press event to scene for processing"""
        self.sceneManager.mouse_button(self.qtButtonIndex.index(event.button()), 0, event.x(), event.y())
        points = iv.pick(self.sceneManager, event.x(), event.y(), pickAll=True)
        for p in points:
            p[2].appearance.material.diffuseColor = (1, 1, 1)


class Window(QtGui.QWidget):
    """Main application window"""
    def __init__(self, parent=None):
        QtGui.QWidget.__init__(self, parent)
        
        widget = QMyPickWidget(format=QtOpenGL.QGLFormat(QtOpenGL.QGL.SampleBuffers))
        widget.sceneManager.background = (0.3, 0.3, 0.3)
        widget.sceneManager.scene = makeRandomScene(150)
        widget.sceneManager.interaction(1)

        mainLayout = QtGui.QHBoxLayout()
        mainLayout.setContentsMargins(0, 0, 0, 0)
        mainLayout.addWidget(widget)
        self.setLayout(mainLayout)
        self.setWindowTitle(self.tr("Random Discs"))

    def minimumSizeHint(self):
        return QtCore.QSize(100, 100)

if __name__ == '__main__':
    app = QtGui.QApplication(sys.argv)
    window = Window()
    window.show()
    sys.exit(app.exec_())

