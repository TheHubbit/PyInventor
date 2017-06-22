/**
 * \file   
 * \brief      PyInventor module entry point implementation.
 * \author     Thomas Moeller
 * \details
 *
 * Copyright (C) the PyInventor contributors. All rights reserved.
 * This file is part of PyInventor, distributed under the BSD 3-Clause
 * License. For full terms see the included COPYING file.
 */


#include <Inventor/SoDB.h>
#include <Inventor/SoLists.h>
#include <Inventor/actions/SoSearchAction.h>
#include <Inventor/actions/SoRayPickAction.h>
#include <Inventor/SoPickedPoint.h>
#include <Inventor/actions/SoWriteAction.h>
#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/actions/SoGetMatrixAction.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/SoInput.h>
#include <Inventor/SoOffscreenRenderer.h>
#include <Inventor/engines/SoGate.h>
#include <Inventor/engines/SoSelectOne.h>
#include <Inventor/engines/SoConcatenate.h>
#include "PySceneObject.h"
#include "PySceneManager.h"
#include "PySensor.h"
#include "PyField.h"
#include "PyEngineOutput.h"
#include "PyPath.h"
#include "PyNodekitCatalog.h"
#include <numpy/ndarrayobject.h>
#include <set>


#ifndef _WIN32
#include <unistd.h>
#else
#include <Windows.h> // for Sleep()
#endif


static PyObject* iv_process_queues(PyObject * /*self*/, PyObject * args)
{
	PySceneObject::initSoDB();

	int idle = true;
	if (PyArg_ParseTuple(args, "|p", &idle))
	{
		SoDB::getSensorManager()->processTimerQueue();
		SoDB::getSensorManager()->processDelayQueue(idle ? TRUE : FALSE);
		if (idle)
		{
			#ifndef _WIN32
			usleep(10000);
			#else
			::Sleep(10);
			#endif
		}
	}

	Py_INCREF(Py_None);
	return Py_None;
}


static PyObject* iv_create_classes(PyObject *self, PyObject * /*args*/)
{
	PySceneObject::initSoDB();
    std::set<SbName> sCreatedWrappers;

	SoTypeList lst;
	SoType::getAllDerivedFrom(SoType::fromName("FieldContainer"), lst);
	for (int i = 0; i < lst.getLength(); ++i)
	{
		if (lst[i].canCreateInstance() && (sCreatedWrappers.find(lst[i].getName()) == sCreatedWrappers.end()))
		{
            sCreatedWrappers.insert(lst[i].getName());
			PyTypeObject *type = PySceneObject::getWrapperType(lst[i].getName().getString());
			if (PyType_Ready(type) >= 0)
			{
				Py_INCREF(type);
				PyModule_AddObject(self, type->tp_name, (PyObject *) type);
			}
		}
	}

	Py_INCREF(Py_None);
	return Py_None;
}


PyObject* iv_classes(PyObject * /*self*/, PyObject *args)
{
	char *baseTypeName = 0;
	if (PyArg_ParseTuple(args, "|s", &baseTypeName))
	{
		SoType baseType = SoType::fromName(baseTypeName ? baseTypeName : "FieldContainer");

		SoTypeList lst;
		SoType::getAllDerivedFrom(baseType, lst);

		// first count total number of objects
		int count = 0;
		for (int i = 0; i < lst.getLength(); ++i)
		{
			if (lst[i].canCreateInstance())
			{
				count++;
			}
		}

		// now add names to list and return
		PyObject *names = PyList_New(count);
		count = 0;

		for (int i = 0; i < lst.getLength(); ++i)
		{
			if (lst[i].canCreateInstance())
			{
				PyList_SetItem(names, count++, PyUnicode_FromString(lst[i].getName().getString()));
			}
		}

		return names;
	}

	Py_INCREF(Py_None);
	return Py_None;
}


PyObject* iv_create_object(PyObject * /*self*/, PyObject *args, PyObject *kwds)
{
    SoFieldContainer *inventorObject = NULL;
    char *type = NULL, *name = NULL, *init = NULL;
    PyObject *pointer = NULL;
    static char *kwlist[] = { "type", "init", "name", "pointer", NULL };

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|sssO", kwlist, &type, &init, &name, &pointer))
    {
        PyErr_SetString(PyExc_TypeError, "Incorrect arguments provided!");
    }
    else if (pointer)
    {
        // pointer provided
        if (PyCapsule_CheckExact(pointer))
        {
            void* ptr = PyCapsule_GetPointer(pointer, "SoFieldContainer");
            if (ptr)
            {
                inventorObject = (SoFieldContainer *)ptr;
            }
        }
    }
    else if (type && type[0])
    {
        // create new instance
        SoType t = SoType::fromName(type);

        // special handling for "templated" types (Gate, Concatenate, SelectOne)
        if (init && t.isDerivedFrom(SoGate::getClassTypeId()))
        {
            inventorObject = new SoGate(SoType::fromName(init));
            init = NULL;
        }
        else if (init && t.isDerivedFrom(SoConcatenate::getClassTypeId()))
        {
            inventorObject = new SoConcatenate(SoType::fromName(init));
            init = NULL;
        }
        else if (init && t.isDerivedFrom(SoSelectOne::getClassTypeId()))
        {
            inventorObject = new SoSelectOne(SoType::fromName(init));
            init = NULL;
        }
        else if (t.canCreateInstance())
        {
            inventorObject = (SoFieldContainer *)t.createInstance();
        }
    }
    else if (name)
    {
        // instance lookup by name
        inventorObject = SoNode::getByName(name);
        if (!inventorObject)
        {
            inventorObject = SoEngine::getByName(name);
        }
        name = NULL;
    }

    if (inventorObject)
    {
        inventorObject->ref();
        if (name && name[0]) inventorObject->setName(name);
        if (init && init[0]) PySceneObject::setFields(inventorObject, init);

        return PySceneObject::createWrapper(inventorObject);
    }

    Py_INCREF(Py_None);
    return Py_None;
}


PyObject* iv_read(PyObject * /*self*/, PyObject *args)
{
	char *iv = 0;
	if (PyArg_ParseTuple(args, "s", &iv))
	{
		if (iv)
		{
			SbBool success = TRUE;
			SoInput in;
			if (iv[0] == '#')
			{
				in.setBuffer(iv, strlen(iv));
			}
			else
			{
				success = in.openFile(iv);
			}

			if (success)
			{
				SoSeparator *root = SoDB::readAll(&in);
				if (root)
				{
					PyObject *scene = PySceneObject::createWrapper(root);
					if (scene)
					{
						return scene;
					}
				}
			}
		}
	}

	Py_INCREF(Py_None);
	return Py_None;
}


PyObject* iv_write(PyObject * /*self*/, PyObject *args, PyObject *kwds)
{
	PyObject *applyTo = NULL;
	char *fileName = NULL;
	static char *kwlist[] = { "applyTo", "file", NULL};
    
	if (PyArg_ParseTupleAndKeywords(args, kwds, "O|s", kwlist, &applyTo, &fileName))
	{
		if (PyNode_Check(applyTo))
		{
			PySceneObject::Object *sceneObj = (PySceneObject::Object *)	applyTo;
			if (sceneObj->inventorObject)
			{
				if (fileName)
				{
					SoOutput out;
					out.openFile(fileName);
					SoWriteAction wa(&out);
					wa.apply((SoNode*) sceneObj->inventorObject);
				}
				else
				{
					void *buffer = malloc(1024 * 1024);
					SoOutput out;
					out.setBuffer(buffer, 1024 * 1024, realloc);
					SoWriteAction wa(&out);
					wa.apply((SoNode*) sceneObj->inventorObject);

					#ifdef getBuffer
					#undef getBuffer
					#endif
					size_t n = 0;
					if (out.getBuffer(buffer, n))
					{
						PyObject *s = PyUnicode_FromStringAndSize((const char*) buffer, n);
						free(buffer);

						return s;
					}
				}
			}
		}
	}

	Py_INCREF(Py_None);
	return Py_None;
}


PyObject* iv_search(PyObject * /*self*/, PyObject *args, PyObject *kwds)
{
	PyObject *applyTo = NULL, *node = NULL;
	char *type = NULL, *name = NULL;
	int searchAll = false, first = true;
	static char *kwlist[] = { "applyTo", "type", "node", "name", "searchAll", "first", NULL};

	if (PyArg_ParseTupleAndKeywords(args, kwds, "O|sOspp", kwlist, &applyTo, &type, &node, &name, &searchAll, &first))
	{
		if (PyNode_Check(applyTo))
		{
			PySceneObject::Object *sceneObj = (PySceneObject::Object *)	applyTo;
			if (sceneObj->inventorObject)
			{
				SoSearchAction sa;
				if (type) sa.setType(SoType::fromName(type));
				if (name) sa.setName(name);
				if (searchAll) sa.setSearchingAll(TRUE);
				sa.setInterest(first ? SoSearchAction::FIRST : SoSearchAction::ALL);
                if (node && PyNode_Check(node))
                {
                    sa.setNode((SoNode*) ((PySceneObject::Object*) node)->inventorObject);
                }
				sa.apply((SoNode*) sceneObj->inventorObject);

				if (first)
				{
					if (sa.getPath())
					{
                        return PyPath::createWrapper(sa.getPath());
					}
				}
				else
				{
					SoPathList pl = sa.getPaths();
					PyObject *found = PyList_New(pl.getLength());
					for (int i = 0; i < pl.getLength(); ++i)
					{
                        PyList_SetItem(found, i, PyPath::createWrapper(pl[i]));
					}
					return found;
				}
			}
		}
	}

	Py_INCREF(Py_None);
	return Py_None;
}


PyObject* iv_pick(PyObject * /*self*/, PyObject *args, PyObject *kwds)
{
	PyObject *applyTo = NULL;
	int pickAll = 0; // don't use bool, crashes on OS X / clang
	int x = -1, y = -1, width = -1, height = -1;
	float nearDist = -1.f, farDist = -1.f;
	PyObject *start = 0, *dir = 0;

	static char *kwlist[] = { "applyTo", "x", "y", "width", "height", "start", "direction", "near", "far", "pickAll", NULL};

	if (PyArg_ParseTupleAndKeywords(args, kwds, "O|iiiiOOffp", kwlist, &applyTo, &x, &y, &width, &height, &start, &dir, &nearDist, &farDist, &pickAll))
	{
		// if scene manager then use scene node and viewport size from there
		SbColor background;
		PySceneManager::getScene(applyTo, applyTo, width, height, background);

		if (PyNode_Check(applyTo))
		{
			PySceneObject::Object *sceneObj = (PySceneObject::Object *)	applyTo;
			if (sceneObj->inventorObject)
			{
				SbViewportRegion viewport;
				if ((width != -1) && (height != -1))
				{
					viewport = SbViewportRegion(SbVec2s(short(width), short(height)));
				}

				SoRayPickAction pa(viewport);

				if ((x != -1) && (y != -1))
				{
					pa.setPoint(SbVec2s(short(x), viewport.getViewportSizePixels()[1] - short(y)));
				}

				float startVec[3], dirVec[3];
				if (!PyField::getFloatsFromPyObject(start, 3, startVec))
				{
					start = 0;
				}
				if (!PyField::getFloatsFromPyObject(dir, 3, dirVec))
				{
					dir = 0;
				}
				if (start && dir)
				{
					pa.setRay(SbVec3f(startVec), SbVec3f(dirVec), nearDist, farDist);
				}

				pa.setPickAll(pickAll);
				pa.apply((SoNode*) sceneObj->inventorObject);

				int numPoints = 0;
				while (pa.getPickedPoint(numPoints)) 
					++numPoints;

				PyObject *points = PyList_New(numPoints);
				for (int i = 0; i < numPoints; ++i)
				{
					SoPickedPoint *p = pa.getPickedPoint(i); 

					PyObject *point = PyList_New(3);
                    PyList_SetItem(point, 0, PyField::getPyObjectArrayFromData(NPY_FLOAT32, p->getPoint().getValue(), 3));
                    PyList_SetItem(point, 1, PyField::getPyObjectArrayFromData(NPY_FLOAT32, p->getNormal().getValue(), 3));

					if (p->getPath())
					{
						PyList_SetItem(point, 2, PyPath::createWrapper(p->getPath()));
					}
					else
					{
						Py_INCREF(Py_None);
						PyList_SetItem(point, 2, Py_None);
					}

					PyList_SetItem(points, i, point);
				}

				return points;
			}
		}
	}

	Py_INCREF(Py_None);
	return Py_None;
}


PyObject* iv_get_matrix(PyObject * /*self*/, PyObject *args, PyObject *kwds)
{
	PyObject *applyTo = NULL;

	static char *kwlist[] = { "applyTo", NULL};
	if (PyArg_ParseTupleAndKeywords(args, kwds, "O", kwlist, &applyTo))
	{
        SbViewportRegion vp;
        SoGetMatrixAction matrixAction(vp);

        if (PyNode_Check(applyTo))
        {
            PySceneObject::Object *sceneObj = (PySceneObject::Object *)	applyTo;
            SoNode *node = (SoNode*)sceneObj->inventorObject;
            if (node)
            {
                matrixAction.apply(node);
            }
        }
        else if (PyObject_TypeCheck(applyTo, PyPath::getType()))
        {
            SoPath *path = PyPath::getInstance(applyTo);
            if (path)
            {
                matrixAction.apply(path);
            }
        }

        SbMatrix matrix = matrixAction.getMatrix();
        return PyField::getPyObjectArrayFromData(NPY_FLOAT32, matrix.getValue(), 4, 4);
	}

	Py_INCREF(Py_None);
	return Py_None;
}


PyObject* iv_render_buffer(PyObject * /*self*/, PyObject *args, PyObject *kwds)
{
	// keep reusing same instance once created
	static SoOffscreenRenderer *offscreenRenderer = 0;

	PyObject *applyTo = NULL;
	int width = -1, height = -1, components = 4;
	char *file = NULL;
	PyObject *background = 0;
	static char *kwlist[] = { "applyTo", "width", "height", "components", "file", "background", NULL};

	if (PyArg_ParseTupleAndKeywords(args, kwds, "O|iiisO", kwlist, &applyTo, &width, &height, &components, &file, &background))
	{
		// if scene manager then use scene node and viewport size from there if undefined
		int vpWidth = -1, vpHeight = -1;
		SbColor backgroundColor(0, 0, 0);
		SoSeparator *gradientBackground = 0;

		PySceneManager::getScene(applyTo, applyTo, vpWidth, vpHeight, backgroundColor, &gradientBackground);

		if (width < 0) width = vpWidth;
		if (width < 0) width = 512;
		if (height < 0) height = vpHeight;
		if (height < 0) height = 512;

		if (PyNode_Check(applyTo))
		{
			PySceneObject::Object *sceneObj = (PySceneObject::Object *)	applyTo;
			if (sceneObj->inventorObject)
			{
				// configure viewport
				SbViewportRegion viewport;
				viewport = SbViewportRegion(SbVec2s(short(width), short(height)));
				viewport.setViewportPixels(0, 0, short(width), short(height));

				if (!offscreenRenderer)
				{
					offscreenRenderer = new SoOffscreenRenderer(viewport);
				}
				else if (offscreenRenderer->getViewportRegion() != viewport)
				{
					offscreenRenderer->setViewportRegion(viewport);
				}

				// configure number of components
				SoOffscreenRenderer::Components c = (SoOffscreenRenderer::Components) components;
				if (offscreenRenderer->getComponents() != c)
				{
					offscreenRenderer->setComponents(c);
				}

				// configure background color
				PySceneManager::getBackgroundFromObject(background, backgroundColor, &gradientBackground);
				offscreenRenderer->setBackgroundColor(backgroundColor);
				if (gradientBackground)
				{
					gradientBackground->addChild((SoNode*) sceneObj->inventorObject);
				}

				// render scene
				if (offscreenRenderer->render(gradientBackground ? gradientBackground : (SoNode*) sceneObj->inventorObject))
				{
					if (gradientBackground)
					{
						gradientBackground->unref();
						gradientBackground = 0;
					}

					if (file)
					{
						// save to file
						SbString ext(file, (int) strlen(file) - 3, -1);
						ext = ext.lower();

						#ifdef TGS_VERSION
						if (ext == "jpg")
						{
							offscreenRenderer->writeToJPEG(file);
						}
						else if (ext == "png")
						{
							offscreenRenderer->writeToPNG(file);
						}
						else if (ext == "bmp")
						{
							offscreenRenderer->writeToBMP(file);
						}
						else if (ext == "rgb")
						{
							offscreenRenderer->writeToRGB(file);
						}
						else if (ext == "tif")
						{
							offscreenRenderer->writeToTIFF(file);
						}
						#else
						offscreenRenderer->writeToFile(file, ext.getString());
						#endif
					}
					else
					{
						// return array
						unsigned char *buffer = offscreenRenderer->getBuffer();
						PyObject *arr = PyField::getPyObjectArrayFromData(NPY_UBYTE, buffer, height, width, components > 1 ? components : 0);
						return arr;
					}
				}
			}
		}

		if (gradientBackground)
		{
			gradientBackground->unref();
			gradientBackground = 0;
		}
	}

	Py_INCREF(Py_None);
	return Py_None;
}


PyObject* iv_render_image(PyObject *self, PyObject *args, PyObject *kwds)
{
    static PyObject *imageModule = 0, *fromArrayFunc = 0;

    if (!fromArrayFunc)
    {
        // Load the module object
        PyObject *moduleName = PyUnicode_FromString("PIL.Image");
        imageModule = PyImport_Import(moduleName);
        Py_DECREF(moduleName);
        if (imageModule)
        {
            fromArrayFunc = PyObject_GetAttrString(imageModule, "fromarray");
        }
        else
        {
            PyErr_SetString(PyExc_TypeError, "PIL module could not be loaded!");
        }
    }

    if (fromArrayFunc)
    {
        // create image form render buffer
        PyObject *image = PyObject_CallFunctionObjArgs(fromArrayFunc, iv_render_buffer(self, args, kwds), NULL);
        if (image)
        {
            // correct flip from OpenGL coordinate system
            PyObject *flipped = PyObject_CallMethod(image, "transpose", "i", 1);
            Py_DECREF(image);
            if (flipped)
                return flipped;
        }
    }

    Py_INCREF(Py_None);
    return Py_None;
}


// External inventor module creation function.
PyMODINIT_FUNC PyInit_inventor(void)
{
    static PyMethodDef iv_methods[] =
    {
        { "process_queues", iv_process_queues, METH_VARARGS,
            "Processes inventor timer and delay queues.\n"
            "\n"
            "Args:\n"
            "    Boolean flag indicating if application is idle.\n"
        },
        { "create_classes", iv_create_classes, METH_VARARGS,
            "Creates Python classes for all registered Inventor scene objects."
        },
        { "classes", iv_classes, METH_VARARGS,
            "Returns all class names registered as Inventor scene objects."
            "\n"
            "Args:\n"
            "    Type name of base class to filter for (optional).\n"
            "\n"
            "Returns:\n"
            "    Type names of all classes matching the given filter."
        },
        { "create_object", (PyCFunction)iv_create_object, METH_VARARGS | METH_KEYWORDS,
            "Creates a scene object instance for a given type, scene object name\n"
            "or existing native pointer.\n"
            "\n"
            "Args:\n"
            "    - type: Scene object type of instance to be created (string).\n"
            "    - name: Name of existing scene object to represented.\n"
            "    - pointer: Native pointer of existing scene object to represented.\n"
            "\n"
            "Returns:\n"
            "    Scene object instance or None."
        },
        { "read", (PyCFunction)iv_read, METH_VARARGS,
            "Reads a scene graph from string or file.\n"
            "\n"
            "Args:\n"
            "    String containing scene itself or file path.\n"
            "\n"
            "Returns:\n"
            "    Root node of scene or None on failure."
        },
        { "write", (PyCFunction)iv_write, METH_VARARGS | METH_KEYWORDS,
            "Writes scene graph to file or string.\n"
            "\n"
            "Args:\n"
            "    applyTo: Node where action is applied.\n"
            "    file: Path to file into which scene is written.\n"
            "\n"
            "Returns:\n"
            "    Written scene as string or None is file argument was provided."
        },
        { "search", (PyCFunction)iv_search, METH_VARARGS | METH_KEYWORDS,
            "Searches for children in a scene with given name or type.\n"
            "\n"
            "Args:\n"
            "    applyTo: Node where action is applied.\n"
            "    type: Search for nodes of given type.\n"
            "    node: Search for a specific node in the scene.\n"
            "    name: Search for node of given name.\n"
            "    searchAll: If True search includes children that are normally not\n"
            "               traversed (hidden by switch).\n"
            "    first: If true search returns only the first child found that\n"
            "           matches the search criteria. Otherwise all matching\n"
            "           children are returned. The default is True.\n"
            "\n"
            "Returns:\n"
            "    List of paths matching search criteria or single path to matching\n"
            "    node if first is set to true.\n"
        },
		{ "pick", (PyCFunction) iv_pick, METH_VARARGS | METH_KEYWORDS,
            "Performs an intersection test of a ray with objects in a scene.\n"
            "\n"
            "Args:\n"
            "    applyTo: Node or SceneManager where action is applied.\n"
            "    x, y: Viewport position of ray.\n"
            "    width, height: Viewport size.\n"
            "    start,direction: Start and direction vector of ray.\n"
            "    near, far: Near and far distance for ray intersection tests.\n"
            "    pickAll: If true returns all objects that intersect with ray.\n"
            "             By default only first intersection is returned.\n"
            "\n"
            "Returns:\n"
            "    List of points, normals and paths for each intersected object.\n"
        },
        { "get_matrix", (PyCFunction)iv_get_matrix, METH_VARARGS | METH_KEYWORDS,
            "Returns the accumulated transforms in a graph or path.\n"
            "\n"
            "Args:\n"
            "    applyTo: Node or path where action is applied.\n"
            "\n"
            "Returns:\n"
            "    Accumulated transformation matrix."
        },
		{ "render_buffer", (PyCFunction) iv_render_buffer, METH_VARARGS | METH_KEYWORDS,
            "Renders a scene into an offscreen buffer using the inventor\n"
			"SoOffscreenRenderer class. Note that this class creates a private,\n"
            "none-shared OpenGL context. If you are using a GUI framework such as\n"
            "Qt then it is better to do the offscreen rendering with classes like\n"
            "QtOpenGL.QGLFramebufferObject because the application controls the\n"
			"setup of the OpenGL context.\n"
            "\n"
            "Args:\n"
            "    applyTo: Node or SceneManager where render action is applied.\n"
            "    width, height: Viewport size.\n"
			"    components: LUMINANCE = 1, LUMINANCE_TRANSPARENCY = 2,\n"
			"                RGB = 3, RGB_TRANSPARENCY = 4\n"
			"    file: Optional file name to write image buffer into.\n"
			"    background: Background color. Provide two colors for\n"
			"                gradient.\n"
            "\n"
            "Returns:\n"
            "    Pixel buffer of rendered scene."
        },
        { "render_image", (PyCFunction)iv_render_image, METH_VARARGS | METH_KEYWORDS,
                "Same as render_buffer() only that the returned buffer is wrapped in\n"
                "an Image object of the Python Imaging Library (PIL).\n"
                "\n"
                "Args:\n"
                "    applyTo: Node or SceneManager where render action is applied.\n"
                "    width, height: Viewport size.\n"
                "    components: LUMINANCE = 1, LUMINANCE_TRANSPARENCY = 2,\n"
                "                RGB = 3, RGB_TRANSPARENCY = 4\n"
                "    file: Optional file name to write image buffer into.\n"
                "    background: Background color. Provide two colors for\n"
                "                gradient.\n"
                "\n"
                "Returns:\n"
                "    Image of rendered scene."
        },
        { NULL, NULL, 0, NULL }
	};

	static struct PyModuleDef iv_module = 
	{
		PyModuleDef_HEAD_INIT,
		"inventor",					/* name of module */
		"Open Inventor 3D Toolkit\n"
        "\n"
        "This module provides capabilities to work with Inventor scene objects. It\n"
        "implements the following classes:\n"
        "- FieldContainer: Base class for all scene objects.\n"
        "- Node: Generic scene object class for nodes.\n"
        "- Engine: Generic scene object class for engines.\n"
        "- SceneManager: Utility class for visualizing and interacting with scene\n"
        "                graphs.\n"
        "- Sensor: Utility class for observing node, field or time changes.\n"
        "- Field: Represents a field instance (needed for connections).\n"
        "- EngineOutput: Represents an output (needed for connections).\n"
        "- Path: Represents a traversal path (return type of search and pick methods).\n"
        "- NodekitCatalog: Describes notekit catalog entries.\n"
        "\n"
        "Furthermore this module creates Python classes for all registered engines\n"
        "and nodes dynamically, thereby enabling access to scene object fields via\n"
        "class attributes.\n"
        ,	/* module documentation, may be NULL */
		-1,							/* size of per-interpreter state of the module, or -1 if the module keeps state in global variables. */
		iv_methods
	};


	PyObject* mod = PyModule_Create(&iv_module);
	if (mod != NULL)
	{
		PyTypeObject *types[] = 
		{
			PySceneManager::getType(),
			PySceneObject::getFieldContainerType(),
			PySceneObject::getNodeType(),
			PySceneObject::getEngineType(),
			PySensor::getType(),
            PyField::getType(),
            PyEngineOutput::getType(),
            PyPath::getType(),
            PyNodekitCatalog::getType(),
			NULL,
		};

		for (int i = 0; types[i]; ++i)
		{
			if (PyType_Ready(types[i]) >= 0)
			{
				Py_INCREF(types[i]);
				PyModule_AddObject(mod, types[i]->tp_name, (PyObject *) types[i]);
			}
		}

		iv_create_classes(mod, NULL);
	}

	return mod;
}

