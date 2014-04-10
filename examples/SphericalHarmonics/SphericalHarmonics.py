#!/usr/bin/env python

"""IVuPY 'Spherical Harmonics' example adapted to PyInventor + PySide
    
    See: http://ivupy.sourceforge.net/examples.html"""

import sys
import numpy
import inventor as iv
from PySide import QtCore, QtGui, QtOpenGL
from PyInventor import QtInventor


class CircularColorMap(object):
    """Circular color map to create a packed color array in view of data
    transfer to Coin3D or Open Inventor.
        
    See: http://astronomy.swin.edu.au/~pbourke/colour/colourramp/
    """
    
    def __init__(self):
        """Create the lookup table.
            
        See: http://astronomy.swin.edu.au/~pbourke/colour/colourramp/
        """
        n = 6*256
        r = numpy.zeros(n, numpy.uint32)
        g = numpy.zeros(n, numpy.uint32)
        b = numpy.zeros(n, numpy.uint32)
        up = numpy.arange(0, 256, 1, numpy.uint32)
        down = numpy.arange(255, -1, -1, numpy.uint32)
        r1, g1, b1 = (0, 1, 1) # cyan
        for i, (r2, g2, b2) in enumerate((
            (0, 0, 1), # blue
            (1, 0, 1), # magenta
            (1, 0, 0), # red
            (1, 1, 0), # yellow
            (0, 1, 0), # green
            (0, 1, 1), # cyan
            )):
            s = slice(i*256, (i+1)*256)
            if r1:
                if r2: r[s] = 255
                else: r[s] = down
            elif r2: r[s] = up
            if g1:
                if g2: g[s] = 255
                else: g[s] = down
            elif g2: g[s] = up
            if b1:
                if b2: b[s] = 255
                else: b[s] = down
            elif b2: b[s] = up
            r1, g1, b1 = r2, g2, b2
                                          
        self.__m = (r << 16)  + (g << 8) + b
        self.__m <<= 8
        self.__m += 255
    
    # __init__()
    
    def colors(self, data, datamin = None, datamax = None):
        """Map data to a packed color array.
        """
        if datamin is None:
            datamin = numpy.minimum.reduce(data)
        if datamax is None:
            datamax = numpy.maximum.reduce(data)
        if datamax < datamin:
            datamax, datamin = datamin, datamax
        indices = ((((len(self.__m)-1)/(datamax-datamin))) * data).astype(numpy.int32)
        return numpy.take(self.__m, indices)

    # colors()

# class CircularColorMap


def sphericalHarmonics(nu, nv, colorMap):
    """Calculate the vertices and colors of a 'spherical harmonics' in view of
    data transfer to Coin3D or Open Inventor.
        
    See: http://astronomy.swin.edu.au/~pbourke/surfaces/sphericalh
    """
    
    # Coin generates a warning when nu and/or nv are even.
    assert(nu % 2 == 1)
    assert(nv % 2 == 1)
    
    i = numpy.arange(nu*nv)
    u = i % nu
    u %= nu
    u = numpy.pi*u/(nu-1)   # phi
    v = i / nu
    v %= nv
    v = 2*numpy.pi*v/(nu-1) # theta
    m = (4, 3, 2, 3, 6, 2, 6, 4)
    
    r = numpy.sin(m[0]*u)**m[1]+numpy.cos(m[2]*u)**m[3]+numpy.sin(m[4]*v)**m[5]+numpy.cos(m[6]*v)**m[7]
    xyzs = numpy.zeros((nu*nv, 3), numpy.float)
    xyzs[:, 0] = r*numpy.sin(u)*numpy.cos(v)
    xyzs[:, 1] = r*numpy.sin(u)*numpy.sin(v)
    xyzs[:, 2] = r*numpy.cos(u)
    
    colors = colorMap.colors(v) #, 0.0, 2*numpy.pi)

    return xyzs, colors

# sphericalHarmonics()


def makeSurface(nu, nv, xyzs, colors):
    """Create a scene graph from vertex and color data."""
    
    result = iv.Separator()
    result += iv.ShapeHints("vertexOrdering COUNTERCLOCKWISE shapeType UNKNOWN_SHAPE_TYPE")

    vertexProperty = iv.VertexProperty("materialBinding PER_VERTEX_INDEXED")
    vertexProperty.orderedRGBA = colors
    vertexProperty.vertex = xyzs

    # Define the QuadMesh.
    quadMesh = iv.QuadMesh()
    quadMesh.verticesPerRow = nu
    quadMesh.verticesPerColumn = nv
    quadMesh.vertexProperty = vertexProperty
    result += quadMesh
    
    return result

# makeSurface()


def makeSphericalHarmonicsScene(index):
    """Returns a spherical harmonics surface surrounded by trackball"""
    nu = 2**(4+index) + 1
    nv = nu
    xyzs, colors = sphericalHarmonics(nu, nv, CircularColorMap())
    surf = makeSurface(nu, nv, xyzs, colors)
    
    root =  iv.Separator()
    root += iv.DirectionalLight()
    root += iv.OrthographicCamera()
    root += iv.TrackballManip()
    root += surf
    return root

# makeSphericalHarmonicsScene()


class Window(QtGui.QWidget):
    """Main application window"""
    def __init__(self, parent=None):
        QtGui.QWidget.__init__(self, parent)
        
        widget = QtInventor.QIVWidget()
        widget.sceneManager.background = (0.3, 0.3, 0.3)
        widget.sceneManager.scene = makeSphericalHarmonicsScene(3)
        widget.sceneManager.scene.view_all()

        mainLayout = QtGui.QHBoxLayout()
        mainLayout.setContentsMargins(0, 0, 0, 0)
        mainLayout.addWidget(widget)
        self.setLayout(mainLayout)
        self.setWindowTitle(self.tr("Spherical Harmonics"))

    def minimumSizeHint(self):
        return QtCore.QSize(100, 100)

if __name__ == '__main__':
    app = QtGui.QApplication(sys.argv)
    window = Window()
    window.show()
    sys.exit(app.exec_())

