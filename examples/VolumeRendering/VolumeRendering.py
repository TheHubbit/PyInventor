#!/usr/bin/env python

"""PyInventor Example: Volume Rendering

   Demonstrates use of VolumeViz extension. 
   Tested only with Coin's SIMVoleon. However, VSG should be very similar."""

import sys
import numpy as np
import inventor as iv
import ctypes
from OpenGL.GL import *
from PySide import QtCore, QtGui, QtOpenGL
from PyInventor import QtInventor



class Window(QtGui.QWidget):
    """Main application window"""
    def __init__(self, parent=None):
        QtGui.QWidget.__init__(self, parent)
        
        baseSceneStr = "#Inventor V2.1 ascii\n \
             Separator { \
               DirectionalLight {}\n \
               OrthographicCamera { orientation 1 0 0 3.1415926 position 0 0 -5 }\n \
               Material { diffuseColor 1 1 1 }\n \
             }"
        volNode = iv.SoVolumeData("fileName 3dhead.vol")

        # transfer function: simple ramp
        cmap = np.ones((4,256))
        cmap[:4] = np.linspace(0, 1, 256)
        
        mainLayout = QtGui.QHBoxLayout()
        mainLayout.setContentsMargins(0, 0, 0, 0)
        mainLayout.setSpacing(2)
        
        widget = QtInventor.QIVWidget()
        widget.sceneManager.background = (0.5, 0.3, 0.3)
        widget.sceneManager.scene = iv.read(baseSceneStr)
        widget.sceneManager.scene += volNode
        widget.sceneManager.scene += iv.SoTransferFunction("colorMapType RGBA predefColorMap NONE")
        widget.sceneManager.scene[-1].colorMap = cmap.T
        widget.sceneManager.scene += iv.SoOrthoSlice("axis Z sliceNumber 52")
        mainLayout.addWidget(widget)
        
        widget = QtInventor.QIVWidget(None, widget)
        widget.sceneManager.background = (0.3, 0.5, 0.3)
        widget.sceneManager.scene = iv.read(baseSceneStr)
        widget.sceneManager.scene += iv.TrackballManip()
        widget.sceneManager.scene += volNode
        widget.sceneManager.scene += iv.SoTransferFunction("colorMapType RGBA predefColorMap NONE remapLow 0x30 remapHigh 0xff")
        widget.sceneManager.scene[-1].colorMap = cmap.T
        widget.sceneManager.scene += iv.SoVolumeRender()
        mainLayout.addWidget(widget)

        self.setLayout(mainLayout)
        self.setWindowTitle(self.tr("Volume Rendering"))

    def minimumSizeHint(self):
        return QtCore.QSize(100, 100)

if __name__ == '__main__':
    app = QtGui.QApplication(sys.argv)
    
    # load VolViz extension
    try:
        lib = ctypes.cdll.LoadLibrary("/Library/Frameworks/VolumeViz.framework/VolumeViz")
    except ImportError:
        # try without full path
        lib = ctypes.cdll.LoadLibrary("VolumeViz")

    # register extensions in SoDB and create classes (unfortunately there is no exported "C" function)
    lib._ZN17SoVolumeRendering4initEv()
    iv.create_classes()

    window = Window()
    window.show()
    sys.exit(app.exec_())

