/**
 * \file   
 * \brief      PyEngineOutput class declaration.
 * \author     Thomas Moeller
 * \details
 *
 * Copyright (C) the PyInventor contributors. All rights reserved.
 * This file is part of PyInventor, distributed under the BSD 3-Clause
 * License. For full terms see the included COPYING file.
 */


#pragma once

#include "PySceneObject.h"

class SoEngineOutput;


class PyEngineOutput
{
public:
	static PyTypeObject *getType();
    void setInstance(SoEngineOutput *field);
    static SoEngineOutput* getInstance(PyObject *self);

private:
	typedef struct 
	{
		PyObject_HEAD
		SoEngineOutput *output;
	} Object;

	// type implementations
	static void tp_dealloc(Object *self);
	static PyObject* tp_new(PyTypeObject *type, PyObject *args, PyObject *kwds);
	static int tp_init(Object *self, PyObject *args, PyObject *kwds);

	// methods
    static PyObject* get_name(Object *self);
    static PyObject* get_type(Object *self);
    static PyObject* get_container(Object *self);
    static PyObject* enable(Object *self, PyObject *args);
    static PyObject* is_enabled(Object *self);
};

