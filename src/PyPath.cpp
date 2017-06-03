/**
 * \file   
 * \brief      PyPath class implementation.
 * \author     Thomas Moeller
 * \details
 *
 * Copyright (C) the PyInventor contributors. All rights reserved.
 * This file is part of PyInventor, distributed under the BSD 3-Clause
 * License. For full terms see the included COPYING file.
 */


#include <Inventor/SoPath.h>
#include <Inventor/nodes/SoNode.h>
#include <Inventor/SbName.h>
#include "PyPath.h"

#pragma warning ( disable : 4127 ) // conditional expression is constant in Py_DECREF
#pragma warning ( disable : 4244 ) // possible loss of data when converting int to short in SbVec2s
#pragma warning ( disable : 4267 ) // possible loss of data in GET/SET macros


PyTypeObject *PyPath::getType()
{
    static PySequenceMethods sequence_methods[] =
    {
        (lenfunc)sq_length,       /* sq_length */
        0,                        /* sq_concat */
        0,                        /* sq_repeat */
        (ssizeargfunc)sq_item,    /* sq_item */
        0,                        /* was_sq_slice */
        0,                        /* sq_ass_item */
        0,                        /* was_sq_ass_slice */
        (objobjproc)sq_contains,  /* sq_contains */
        0,                        /* sq_inplace_concat */
        0                         /* sq_inplace_repeat */
    };

	static PyTypeObject pathType = 
	{
		PyVarObject_HEAD_INIT(NULL, 0)
		"Path",                    /* tp_name */
		sizeof(Object),            /* tp_basicsize */
		0,                         /* tp_itemsize */
		(destructor) tp_dealloc,   /* tp_dealloc */
		0,                         /* tp_print */
		0,                         /* tp_getattr */
		0,                         /* tp_setattr */
		0,                         /* tp_reserved */
		0,                         /* tp_repr */
		0,                         /* tp_as_number */
        sequence_methods,          /* tp_as_sequence */
        0,                         /* tp_as_mapping */
        0,                         /* tp_hash  */
		0,                         /* tp_call */
		0,                         /* tp_str */
        0,                         /* tp_getattro */
        0,                         /* tp_setattro */
		0,                         /* tp_as_buffer */
		Py_TPFLAGS_DEFAULT |
		Py_TPFLAGS_BASETYPE,       /* tp_flags */
        "Represents a traversal path.\n"
        "\n"
        "This object describes a traversal path and is used as return type\n"
        "of search actions.\n",    /* tp_doc */
		0,                         /* tp_traverse */
		0,                         /* tp_clear */
        (richcmpfunc)tp_richcompare, /* tp_richcompare */
        0,                         /* tp_weaklistoffset */
		0,                         /* tp_iter */
		0,                         /* tp_iternext */
		0,                         /* tp_methods */
		0,                         /* tp_members */
		0,                         /* tp_getset */
		0,                         /* tp_base */
		0,                         /* tp_dict */
		0,                         /* tp_descr_get */
		0,                         /* tp_descr_set */
		0,                         /* tp_dictoffset */
		(initproc) tp_init,        /* tp_init */
		0,                         /* tp_alloc */
		tp_new,                    /* tp_new */
	};

	return &pathType;
}


void PyPath::tp_dealloc(Object* self)
{
	if (self->path)
	{
        self->path->unref();
	}

	Py_TYPE(self)->tp_free((PyObject*)self);
}


PyObject* PyPath::tp_new(PyTypeObject *type, PyObject* /*args*/, PyObject* /*kwds*/)
{
	Object *self = (Object *)type->tp_alloc(type, 0);
	if (self != NULL) 
	{
		self->path = 0;
	}

	return (PyObject *) self;
}


int PyPath::tp_init(Object * /*self*/, PyObject * /*args*/, PyObject * /*kwds*/)
{
	Py_INCREF(Py_None);

	return 0;
}


PyObject *PyPath::tp_richcompare(Object *a, PyObject *b, int op)
{
    PyObject *returnObject = Py_NotImplemented;
    if (PyObject_TypeCheck(b, PyPath::getType()))
    {
        bool result = false;
        SoPath *otherPath = ((Object*)b)->path;

        if (!otherPath || !a->path)
        {
            return NULL;
        }

        switch (op)
        {
        case Py_EQ:	result = *(a->path) == *otherPath ? true : false; break;
        case Py_NE:	result = *(a->path) != *otherPath ? true : false; break;
        default: 
            // unsupported
            return NULL;
        }

        returnObject = result ? Py_True : Py_False;
    }

    Py_INCREF(returnObject);
    return  returnObject;
}


void PyPath::setInstance(SoPath *path)
{
    Object *self = (Object *) this;
    if (self->path)
    {
        self->path->unref();
    }
    self->path = path;
    if (self->path)
    {
        self->path->ref();
    }
}


SoPath *PyPath::getInstance(PyObject *self)
{
    if (PyObject_TypeCheck(self, getType()))
    {
        return ((Object*)self)->path;
    }
    
    return 0;
}


PyObject *PyPath::createWrapper(SoPath *path)
{
    PyObject *obj = PyObject_CallObject((PyObject*) getType(), NULL);
    if (obj)
    {
        ((PyPath*)obj)->setInstance(path);
    }
    return obj;
}


Py_ssize_t PyPath::sq_length(Object *self)
{
    if (self->path)
    {
        return self->path->getLength();
    }

    return 0;
}


int PyPath::sq_contains(Object *self, PyObject *item)
{
    if (item && PyNode_Check(item))
    {
        PySceneObject::Object *child = (PySceneObject::Object*) item;
        return self->path->containsNode((SoNode*)child->inventorObject);
    }

    return 0;
}


PyObject *PyPath::sq_item(Object *self, Py_ssize_t idx)
{
    if (self->path)
    {
        if (idx < self->path->getLength())
        {
            SoNode *node = self->path->getNode(idx);
            PyObject *obj = PySceneObject::createWrapper(node);
            if (obj)
            {
                return obj;
            }
        }
        else
        {
            PyErr_SetString(PyExc_IndexError, "Out of range");
        }
    }
    else
    {
        // don't report error here and just return NULL so "for child in sceneObject:" can be
        // called without checking for group type first
    }

    return NULL;
}
