/**
 * \file   
 * \brief      PyField class declaration.
 * \author     Thomas Moeller
 * \details
 *
 * Copyright (C) the PyInventor contributors. All rights reserved.
 * This file is part of PyInventor, distributed under the BSD 3-Clause
 * License. For full terms see the included COPYING file.
 */


#pragma once

#include "PySceneObject.h"

class SoField;


class PyField
{
public:
	static PyTypeObject *getType();
    void setInstance(SoField *field);

    // helper methods for converting C into Python arrays
    static bool initNumpy();
    static bool getFloatsFromPyObject(PyObject *obj, int size, float *value_out);
    static PyObject *getPyObjectArrayFromData(int type, const void* data, int dim1, int dim2 = 0, int dim3 = 0);

    // helper methods to set/get field values
    static PyObject *getFieldValue(SoField *field);
    static int setFieldValue(SoField *field, PyObject *value);

private:
	typedef struct 
	{
		PyObject_HEAD
		SoField *field;
	} Object;

	// type implementations
	static void tp_dealloc(Object *self);
	static PyObject* tp_new(PyTypeObject *type, PyObject *args, PyObject *kwds);
	static int tp_init(Object *self, PyObject *args, PyObject *kwds);
    static PyObject* tp_getattro(Object *self, PyObject *attrname);
    static int tp_setattro(Object *self, PyObject *attrname, PyObject *value);

	// methods
    static PyObject* connect_from(Object *self, PyObject *args);
    static PyObject* disconnect(Object *self, PyObject *args);
    static PyObject* is_connected(Object *self);
    static PyObject* touch(Object *self);
    static PyObject* get_name(Object *self);
    static PyObject* get_type(Object *self);
};

