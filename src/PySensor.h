/**
 * \file   
 * \brief      PySensor class declaration.
 * \author     Thomas Moeller
 * \details
 *
 * Copyright (C) the PyInventor contributors. All rights reserved.
 * This file is part of PyInventor, distributed under the BSD 3-Clause
 * License. For full terms see the included COPYING file.
 */


#pragma once

#include "PySceneObject.h"

class SoSensor;


class PySensor
{
public:
	static PyTypeObject *getType();

private:
	typedef struct 
	{
		PyObject_HEAD
		SoSensor *sensor;
		PyObject *callback;
	} Object;

	// type implementations
	static void tp_dealloc(Object *self);
	static PyObject* tp_new(PyTypeObject *type, PyObject *args, PyObject *kwds);
	static int tp_init(Object *self, PyObject *args, PyObject *kwds);
	//static int tp_setattro(Object* self, PyObject *attrname, PyObject *value);

	// methods
	static PyObject* attach(Object *self, PyObject *args);
	static PyObject* detach(Object *self);
	static PyObject* setinterval(Object *self, PyObject *args);
	static PyObject* settime(Object *self, PyObject *args);
	static PyObject* schedule(Object *self);
	static PyObject* unschedule(Object *self);
	static PyObject* isscheduled(Object *self);

	// internal
	static void sensorCBFunc(void *userdata, SoSensor *sensor);
};

