#!/usr/bin/env python

"""PyInventor 'Manipulators' example"""

import inventor as iv
from PySide import QtCore, QtGui, QtOpenGL
from PyInventor import QtInventor


class ApplicationWidget(QtGui.QWidget):
    """Main application window"""
    def __init__(self, parent=None):
        QtGui.QWidget.__init__(self, parent)

        # create main application layout
        mainLayout = QtGui.QHBoxLayout()
        mainLayout.setContentsMargins(0, 0, 0, 0)
        self.ivWidget = QtInventor.QIVWidget()
        mainLayout.addWidget(self.ivWidget)
        self.setLayout(mainLayout)
        self.setWindowTitle(self.tr("Manipulators"))

    def createScene(self):
        self.ivWidget.sceneManager.scene = iv.read(__file__.replace(".py", ".iv"))
        self.manip = iv.TransformBoxManip()
        
        # find selection node and attach callbacks
        selectionNode = iv.search(self.ivWidget.sceneManager.scene, type="Selection")[-1]
        self.selectionSensor = iv.Sensor(selectionNode, "selection", self.selectionCB)
        self.deselectionSensor = iv.Sensor(selectionNode, "deselection", self.deselectionCB)
    
    def selectionCB(self, path):
        # attach a manipulator to shapes
        if path[-1].get_type() in ['Cone', 'Sphere', 'Cube']:
            self.manip.replace_node(path)
    
    def deselectionCB(self, path):
        # remove manipulator
        self.manip.replace_manip(iv.search(self.ivWidget.sceneManager.scene, node=self.manip))
    

if __name__ == '__main__':
    app = QtGui.QApplication([])
    window = ApplicationWidget()
    window.show()
    window.createScene()
    app.exec_()
