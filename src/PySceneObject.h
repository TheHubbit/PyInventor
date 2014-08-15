/**
 * \file   
 * \brief      PySceneObject class declaration.
 * \author     Thomas Moeller
 * \details
 *
 * Copyright (C) the PyInventor contributors. All rights reserved.
 * This file is part of PyInventor, distributed under the BSD 3-Clause
 * License. For full terms see the included COPYING file.
 */


#pragma once

#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION

#ifdef _DEBUG
	#undef _DEBUG
	#include <Python.h>
	#define _DEBUG
#else
	#include <Python.h>
#endif

#include <structmember.h>

#define PyNode_Check(obj)			PyObject_TypeCheck(obj, PySceneObject::getNodeType())
#define PyEngine_Check(obj)			PyObject_TypeCheck(obj, PySceneObject::getEngineType())
#define PySceneObject_Check(obj)	PyObject_TypeCheck(obj, PySceneObject::getFieldContainerType())


class SoFieldContainer;


class PySceneObject
{
public:
	static PyTypeObject *getFieldContainerType();
	static PyTypeObject *getNodeType();
	static PyTypeObject *getEngineType();
	static PyTypeObject *getWrapperType(const char *typeName, PyTypeObject *baseType = 0);
	
	static PyObject *createWrapper(const char *typeName, SoFieldContainer *instance = 0);

	static PyObject *getField(SoField *field);
	static int setField(SoField *field, PyObject *value);
	static int setFields(SoFieldContainer *fieldContainer, char *value);

	static bool getFloatsFromPyObject(PyObject *obj, int size, float *value_out);
	static PyObject *getPyObjectFromFloats(const float *value_in, int size);

	static bool initNumpy();
	static void initSoDB();

	typedef struct 
	{
		PyObject_HEAD
		SoFieldContainer *inventorObject;
	} Object;

private:
	// type implementations
	static void tp_dealloc(Object *self);
	static PyObject* tp_new(PyTypeObject *type, PyObject *args, PyObject *kwds);
	static int tp_init(Object *self, PyObject *args, PyObject *kwds);
	static int tp_init2(Object *self, PyObject *args, PyObject *kwds);
	static PyObject* tp_getattro(Object *self, PyObject *attrname);
	static int tp_setattro(Object *self, PyObject *attrname, PyObject *value);
	static void initDictionary(Object *self);

	// sequence implementation
	static Py_ssize_t sq_length(Object *self);
	static PyObject * sq_concat(Object *self, PyObject *item);
	static PyObject * sq_inplace_concat(Object *self, PyObject *item);
	static int sq_contains(Object *self, PyObject *item);
	static PyObject *sq_item(Object *self, Py_ssize_t idx);
	static int sq_ass_item(Object *self, Py_ssize_t idx, PyObject *item);

	// mapping implementations
	static Py_ssize_t mp_length(Object *self);
	static PyObject * mp_subscript(Object *self, PyObject *item);
	static int mp_ass_subscript(Object *self, PyObject *key, PyObject *value);

	// list methods
	static PyObject* append(Object *self, PyObject *args);
	static PyObject* insert(Object *self, PyObject *args);
	static PyObject* remove(Object *self, PyObject *args);

	// inventor methods
	static PyObject* getname(Object *self);
	static PyObject* setname(Object *self, PyObject *args);
	static PyObject* sotype(Object *self);
	static PyObject* check_type(Object *self, PyObject *args);
	static PyObject* node_id(Object *self);
	static PyObject* touch(Object *self);
	static PyObject* enable_notify(Object *self, PyObject *args);

	// field methods
	static PyObject* connect(Object *self, PyObject *args);
	static PyObject* disconnect(Object *self, PyObject *args);
	static PyObject* isconnected(Object *self, PyObject *args);
	static PyObject* set(Object *self, PyObject *args);
	static PyObject* get(Object *self, PyObject *args);
    static PyObject* getfields(Object *self);

    // generic field container
    static PyObject* internal_pointer(Object *self);

	// internal methods
	static void setInstance(Object *self, SoFieldContainer *obj);
};

