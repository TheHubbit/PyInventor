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
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/SoInput.h>
#include "PySceneObject.h"
#include "PySceneManager.h"
#include "PySensor.h"


#ifndef _WIN32
#include <unistd.h>
#endif


static PyObject* iv_process_queues(PyObject * /*self*/, PyObject * args)
{
	PySceneObject::initSoDB();

	bool idle = true;
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

	SoTypeList lst;
	SoType::getAllDerivedFrom(SoType::fromName("Node"), lst);
	for (int i = 0; i < lst.getLength(); ++i)
	{
		if (lst[i].canCreateInstance() && !PySceneObject::getWrapperType(lst[i].getName().getString()))
		{
			PyTypeObject *type = PySceneObject::getWrapperType(lst[i].getName().getString(), PySceneObject::getNodeType());
			if (PyType_Ready(type) >= 0)
			{
				Py_INCREF(type);
				PyModule_AddObject(self, type->tp_name, (PyObject *) type);
			}
		}
	}

	SoType::getAllDerivedFrom(SoType::fromName("Engine"), lst);
	for (int i = 0; i < lst.getLength(); ++i)
	{
		if (lst[i].canCreateInstance() && !PySceneObject::getWrapperType(lst[i].getName().getString()))
		{
			PyTypeObject *type = PySceneObject::getWrapperType(lst[i].getName().getString(), PySceneObject::getEngineType());
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
					PyObject *scene = PySceneObject::createWrapper(root->getTypeId().getName().getString(), root);
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
	PyObject *applyTo = NULL;
	char *type = NULL, *name = NULL;
	bool searchAll = false, first = false;
	static char *kwlist[] = { "applyTo", "type", "name", "searchAll", "first", NULL};

	if (PyArg_ParseTupleAndKeywords(args, kwds, "O|sspp", kwlist, &applyTo, &type, &name, &searchAll, &first))
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
				sa.apply((SoNode*) sceneObj->inventorObject);

				if (first)
				{
					if (sa.getPath())
					{
						PyObject *found = PySceneObject::createWrapper(sa.getPath()->getTail()->getTypeId().getName().getString(), sa.getPath()->getTail());
						if (found)
						{
							return found;
						}
					}
				}
				else
				{
					SoPathList pl = sa.getPaths();
					PyObject *found = PyList_New(pl.getLength());
					for (int i = 0; i < pl.getLength(); ++i)
					{
						PyObject *obj = PySceneObject::createWrapper(pl[i]->getTail()->getTypeId().getName().getString(), pl[i]->getTail());
						if (obj)
						{
							PyList_SetItem(found, i, obj);
						}
					}
					return found;
				}
			}
		}

		if (!first)
		{
			// need to return empty list
			return PyList_New(0);
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
		PySceneManager::getScene(applyTo, applyTo, width, height);

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
				if (!PySceneObject::getFloatsFromPyObject(start, 3, startVec))
				{
					start = 0;
				}
				if (!PySceneObject::getFloatsFromPyObject(dir, 3, dirVec))
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
					PyObject *coord = PyTuple_New(3);
					PyTuple_SetItem(coord, 0, PyFloat_FromDouble(p->getPoint()[0]));
					PyTuple_SetItem(coord, 1, PyFloat_FromDouble(p->getPoint()[1]));
					PyTuple_SetItem(coord, 2, PyFloat_FromDouble(p->getPoint()[2]));
					PyList_SetItem(point, 0, coord);

					PyObject *norm = PyTuple_New(3);
					PyTuple_SetItem(norm, 0, PyFloat_FromDouble(p->getNormal()[0]));
					PyTuple_SetItem(norm, 1, PyFloat_FromDouble(p->getNormal()[1]));
					PyTuple_SetItem(norm, 2, PyFloat_FromDouble(p->getNormal()[2]));
					PyList_SetItem(point, 1, norm);

					SoNode *node = 0;
					if (p->getPath())
					{
						node = p->getPath()->getTail();
					}

					if (node)
					{
						PyObject *obj = PySceneObject::createWrapper(p->getPath()->getTail()->getTypeId().getName().getString(), p->getPath()->getTail());
						if (obj)
						{
							PyList_SetItem(point, 2, obj);
						}
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
		{ "read", (PyCFunction) iv_read, METH_VARARGS,
            "Reads a scene graph from string or file.\n"
            "\n"
            "Args:\n"
            "    String containing scene itself or file path.\n"
            "\n"
            "Returns:\n"
            "    Root node of scene or None on failure."
        },
		{ "write", (PyCFunction) iv_write, METH_VARARGS | METH_KEYWORDS,
            "Writes scene graph to file or string.\n"
            "\n"
            "Args:\n"
            "    applyTo: Node where action is applied.\n"
            "    file: Path to file into which scene is written.\n"
            "\n"
            "Returns:\n"
            "    Written scene as string or None is file argument was provided."
        },
		{ "search", (PyCFunction) iv_search, METH_VARARGS | METH_KEYWORDS,
            "Searches for children in a scene with given name or type.\n"
            "\n"
            "Args:\n"
            "    applyTo: Node where action is applied.\n"
            "    type: Search for nodes of given type.\n"
            "    name: Search for node of given name.\n"
            "    searchAll: If True search includes children that are normally not traversed\n"
            "               (hidden by switch).\n"
            "    first: If true search returns only the first child found that matches the\n"
            "           search criteria. By default all matching children are returned.\n"
            "\n"
            "Returns:\n"
            "    List of nodes matching search criteria or first node found."
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
            "    List of points, normals and nodes for each intersected object."
        },
		{ NULL, NULL, 0, NULL }
	};

	static struct PyModuleDef iv_module = 
	{
		PyModuleDef_HEAD_INIT,
		"inventor",					/* name of module */
		"Open Inventor 3D Toolkit\n"
        "\n"
        "This module provides capabilities to work with Inventor scene objects. It implements\n"
        "the following classes:\n"
        "- FieldContainer: Base class for all scene objects.\n"
        "- Node: Generic scene object class for nodes.\n"
        "- Engine: Generic scene object class for engines.\n"
        "- SceneManager: Utility class for visualizing and interacting with scene graphs.\n"
        "- Sensor: Utility class for observing node, field or time changes.\n"
        "\n"
        "Furthermore this module creates Python classes for all registered engines and nodes\n"
        "dynamically, thereby enabling access to scene object fields via class attributes.\n"
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
