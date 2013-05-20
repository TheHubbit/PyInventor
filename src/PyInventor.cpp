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



// External inventor module creation function.
PyMODINIT_FUNC PyInit_inventor(void)
{
	static PyMethodDef iv_methods[] = 
	{
		{ "process_queues", iv_process_queues, METH_VARARGS, "Processes inventor queues" },
		{ "create_classes", iv_create_classes, METH_VARARGS, "Creates Python classes for Inventor scene objects" },
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

