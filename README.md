PyInventor
==========

PyInventor provides a Python binding for the Open Inventor 3D graphics toolkit. The main drivers for this project are:
- Enable the creation, modification and viewing of scene graphs with Python
- Offer a flexible yet easy to use “Python-like” interface to Open Inventor
- Support of Open Inventor extensions without introducing compile time dependencies
- Platform independent and compatible with all major implementations of Open Inventor  (SGI, VSG and Coin)
- Undemanding maintenance of the library
- Compatible with Python 3

## Building
The binding can be compiled and installed using included distutils setup script. To compile the module CPython 3.3, numpy and Open Inventor need to be installed on the target system. Before running the setup, some variables that point to the Inventor include and library paths as well as the library names need to be adjusted.
```
# Open Inventor paths and libraries:
oivincpath = ''
oivlibpath = ''
oivlibs = []
```

Afterwards calling the following command will first compile PyInventor and then copy the binaries into the site-packages directory.
```
python setup.py install
```

## Using the Module
This module provides capabilities to work with Inventor scene objects from Python. The module creates wrapper classes for all engines and nodes whose properties are mapped to each scene object’s fields. Group nodes furthermore support the Python list object API, allowing accessing or modifying children in a similar fashion to working with list items. Additionally a sensor class enables the use of Inventor node, field, alarm and timer sensors. Last a scene manager class provides means for visualizing a scene graph in an Open GL window.

### Hello, Cone
To get started the following code snipped shows how to create a window with GLUT, build a 3D scene and then visualize it in a window using a scene manager.

```python
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
    glutInit()
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
    sm.scene.view_all()

    # running application main loop
    glutMainLoop()
```

Running the above example will display a window whose content is similar to the following image.
![Cone screenshot](http://oivdoc92.vsg3d.com/sites/default/files/imported/mentor/images/2-2_ConeTrackball.gif)

### Module Initialization
The PyInventor module is loaded and initialized with the import statement. The import also loads and initializes the numpy module and the Open Inventor library. It furthermore creates all needed wrapper classes for Python. Afterwards scene objects can be created by simply instantiating the Python classes from the inventor namespace.

```python
>>> import inventor
>>> inventor.Sphere()
<Sphere object at 0x0000000002297C90>
```

Open Inventor is an extendable toolkit, meaning new types can be added to the existing scene object database and used to build scene graphs. In order to work with the new types in Python create_classes() needs to be called after loading an extension library as shown next.
```python
>>> import inventor
>>> import ctypes
>>> ctypes.cdll.LoadLibrary("xipivcore").initAllClasses()
48
>>> inventor.SoXipExaminer() # unknown type before create_classes()
Traceback (most recent call last):
  File "<pyshell#13>", line 1, in <module>
    inventor.SoXipExaminer()
AttributeError: 'module' object has no attribute 'SoXipExaminer'
>>> inventor.create_classes()
>>> inventor.SoXipExaminer() # now all xipivcore types are available
<SoXipExaminer object at 0x00000000021B7C78>
```

### Nodes and Engines
Upon initialization or calling create_classes() Python classes are created for all scene objects derived from SoNode and SoEngine. Scene objects are created by instantiating the corresponding Python wrapper class. Optionally two string arguments can be provided to the constructor. The first on is an initialization string that results in a set() call to the created scene object. The second parameter is an optional scene object name.

#### Fields
The fields of a scene objects can be accessed as dynamic properties in Python.

```python
>>> sphere = inventor.Sphere("radius 4")
>>> sphere.radius
4.0
>>> sphere.radius = 2
>>> sphere.radius
2.0
```

Using the connect() method a field connection from another object’s field can be established.

```python
>>> sphere = inventor.Sphere()
>>> cone = inventor.Cone()
>>> cone.connect("height", sphere, "radius")
True
>>> sphere.radius = 3
>>> cone.height
3.0
```

A connection can be undone with a call to disconnect() on the target object.

#### Groups
Group nodes act much like lists in Python. That means children can be accessed with the [ ] operator. In addition group nodes have append(), insert() and remove() methods.

```python
>>> group = inventor.Group()
>>> group[0] = inventor.Cone()
>>> group += inventor.Sphere()
>>> group.append(inventor.Cube())
>>> for i in group: print(i)

<Cone object at 0x0000000007A8D6F0>
<Sphere object at 0x0000000007A8D6D8>
<Cube object at 0x0000000007A8D6F0>
>>> del (group[1])
>>> len (group)
2
```

Working with scene graphs it is a common use case to insert nodes to a group relative to another child node. To address this use case the insert() method allows for an optional third parameter specifying a child of a group. The node is then inserted relative to the existing child provided in the third argument. In the example below a Material node is added at the location of the Cone node.

```python
>>> sphere = inventor.Sphere()
>>> cone = inventor.Cone()
>>> group = inventor.Group()
>>> group += (sphere, cone)
>>> group.insert(0, inventor.Material("diffuseColor 1 0 0"), cone)
>>> group[1]
<Material object at 0x0000000007A8D7F8>
```

#### Actions
Inventor actions are not provided as separate classes in Python but are instead made available as methods of the node classes itself. Currently those actions are supported:
- **view_all():** Searches for a camera child and initializes it so all objects in the scene are visible.
- **read():** Reads a scene graph from file (*.iv) or string.
- **write():** Writes a scene into a file whose path is provided as argument or returns serialized scene as as string if no argument is given.
- **search():** Searches for children of given type or name. This methods supports the following named parameters.
  - type: Specifies a object type to look for.
  - name: Specifies a object name to look for.
  - searchAll: If true search includes children that are normally not traversed (hidden by switch).
  - first: If true search returns only the first child found that matches the search criteria. By default returns a list of all matching children.

### Sensors
Sensors are accessed from Python through a single sensor class. Different methods are provided for creating node, field, timer or alarm sensors. Note that sensors are managed and called from queues and it is necessary for an application to call process_queues() regularly. Once a sensor is triggered it calls the functions assigned to the “callback” property.
- **attach():** Attaches a sensor to changes in a node or a specific field of a node (optional second paramter is the field name).
- **setinterval():** Initializes a reoccuring timer sensor with the given interval (double number initialization for SbTime). This sensor must be activated using the schedule() method.
- **settime():** Initializes an alarm sensor that is triggered after the specified amount of time has elapsed (double number initialization for SbTime). This sensor must be activated using the schedule() method.

### Scene Manager
The scene manager class handles the display and event processing of a scene and can easily be linked to the GLUT library as shown in the ‘Hello, Cone’ example above.

## Links
- SGI Open Inventor (open source): http://oss.sgi.com/projects/inventor/
- VSG Open Inventor (commercial implementation) developer zone: http://oivdoc92.vsg3d.com/
- Open Inventor extensions for medical imaging: https://collab01a.scr.siemens.com/xipwiki/index.php/Main_Page

