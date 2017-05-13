/**
 * \file   
 * \brief      PyPath class declaration.
 * \author     Thomas Moeller
 * \details
 *
 * Copyright (C) the PyInventor contributors. All rights reserved.
 * This file is part of PyInventor, distributed under the BSD 3-Clause
 * License. For full terms see the included COPYING file.
 */


#pragma once

#include "PySceneObject.h"

class SoPath;


class PyPath
{
public:
	static PyTypeObject *getType();
    void setInstance(SoPath *path);
    static SoPath* getInstance(PyObject *self);
    static PyObject *createWrapper(SoPath *path);

private:
	typedef struct 
	{
		PyObject_HEAD
		SoPath *path;
	} Object;

	// type implementations
	static void tp_dealloc(Object *self);
	static PyObject* tp_new(PyTypeObject *type, PyObject *args, PyObject *kwds);
	static int tp_init(Object *self, PyObject *args, PyObject *kwds);
    static PyObject *tp_richcompare(Object *a, PyObject *b, int op);

    // sequence implementation
    static Py_ssize_t sq_length(Object *self);
    static int sq_contains(Object *self, PyObject *item);
    static PyObject *sq_item(Object *self, Py_ssize_t idx);
};

