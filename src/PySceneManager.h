/**
 * \file   
 * \brief      PySceneManager class declaration.
 * \author     Thomas Moeller
 * \details
 *
 * Copyright (C) the PyInventor contributors. All rights reserved.
 * This file is part of PyInventor, distributed under the BSD 3-Clause
 * License. For full terms see the included COPYING file.
 */


#pragma once

#include "PySceneObject.h"

class SoSceneManager;

// for VSG Inventor use of context class is required
class SoGLContext;


class PySceneManager
{
public:
	static PyTypeObject *getType();

private:
	typedef struct 
	{
		PyObject_HEAD
		SoSceneManager *sceneManager;
		PyObject *scene;
		PyObject *renderCallback;
		PyObject *backgroundColor;
		SoGLContext *context;
	} Object;

	// type implementations
	static void tp_dealloc(Object *self);
	static PyObject* tp_new(PyTypeObject *type, PyObject *args, PyObject *kwds);
	static int tp_init(Object *self, PyObject *args, PyObject *kwds);
	static int tp_setattro(Object* self, PyObject *attrname, PyObject *value);

	// methods
	static PyObject* render(Object *self);
	static PyObject* resize(Object *self, PyObject *args);
	static PyObject* mouse_button(Object *self, PyObject *args);
	static PyObject* mouse_move(Object *self, PyObject *args);
	static PyObject* key(Object *self, PyObject *args);
	static PyObject* view_all(Object *self, PyObject *args);

	// internal
	static void renderCBFunc(void *userdata, SoSceneManager *mgr);
};

