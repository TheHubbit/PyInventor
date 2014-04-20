#!/usr/bin/env python

"""PyInventor 'Hello Cone' example using GLUT"""

from OpenGL.GL import *
from OpenGL.GLUT import *
import inventor as iv

def create_scene():
    '''Returns a simple scene (light, camera, manip, material, cone)'''
    root = iv.Separator()
    root += iv.DirectionalLight()
    root += iv.OrthographicCamera()
    root += iv.TrackballManip()
    root += iv.Material("diffuseColor 1 0 0")
    root += iv.Cone()
    return root    

if __name__ == '__main__':
    # initialize GLUT and create a window with scene manager
    glutInit([b""])
    glutInitWindowSize(512, 512)
    glutCreateWindow(b"Trackball")
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB)
    glEnable(GL_DEPTH_TEST)
    glEnable(GL_LIGHTING)
    sm = iv.SceneManager()
    sm.redisplay = glutPostRedisplay
    glutReshapeFunc(sm.resize)
    glutDisplayFunc(sm.render)
    glutMouseFunc(sm.mouse_button)
    glutMotionFunc(sm.mouse_move)
    glutKeyboardFunc(sm.key)
    glutIdleFunc(iv.process_queues)
    
    # setup scene
    sm.scene = create_scene()
    sm.view_all()
    
    # running application main loop
    glutMainLoop()
