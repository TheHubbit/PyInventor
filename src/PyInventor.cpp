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


PyObject* iv_write(PyObject * /*self*/, PyObject *args)
{
	PyObject *applyTo = NULL;
	char *iv = 0;
	if (PyArg_ParseTuple(args, "O|s", &applyTo, &iv))
	{
		if (PyNode_Check(applyTo))
		{
			PySceneObject::Object *sceneObj = (PySceneObject::Object *)	applyTo;
			if (sceneObj->inventorObject)
			{
				if (iv)
				{
					SoOutput out;
					out.openFile(iv);
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


// External inventor module creation function.
PyMODINIT_FUNC PyInit_inventor(void)
{
	static PyMethodDef iv_methods[] = 
	{
		{ "process_queues", iv_process_queues, METH_VARARGS, "Processes inventor queues" },
		{ "create_classes", iv_create_classes, METH_VARARGS, "Creates Python classes for Inventor scene objects" },
		{ "read", (PyCFunction) iv_read, METH_VARARGS, "Reads a scene graph" },
		{ "write", (PyCFunction) iv_write, METH_VARARGS, "Writes scene graph to file" },
		{ "search", (PyCFunction) iv_search, METH_VARARGS | METH_KEYWORDS, "Searches all children for a specific scene object" },
		{ NULL, NULL, 0, NULL }
	};

	static struct PyModuleDef iv_module = 
	{
		PyModuleDef_HEAD_INIT,
		"inventor",					/* name of module */
		"Open Inventor 3D Toolkit",	/* module documentation, may be NULL */
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

