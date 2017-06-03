/**
* \file
* \brief      PyNodekitCatalog class implementation.
* \author     Thomas Moeller
* \details
*
* Copyright (C) the PyInventor contributors. All rights reserved.
* This file is part of PyInventor, distributed under the BSD 3-Clause
* License. For full terms see the included COPYING file.
*/


#include <Inventor/nodekits/SoNodekitCatalog.h>
#include <Inventor/nodes/SoNode.h>
#include <Inventor/SbName.h>
#include "PyNodekitCatalog.h"

#pragma warning ( disable : 4127 ) // conditional expression is constant in Py_DECREF
#pragma warning ( disable : 4244 ) // possible loss of data when converting int to short in SbVec2s
#pragma warning ( disable : 4267 ) // possible loss of data in GET/SET macros


PyTypeObject *PyNodekitCatalog::getType()
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

    static PyTypeObject catalogType =
    {
        PyVarObject_HEAD_INIT(NULL, 0)
        "NodekitCatalog",          /* tp_name */
        sizeof(Object),            /* tp_basicsize */
        0,                         /* tp_itemsize */
        (destructor)tp_dealloc,    /* tp_dealloc */
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
        "Represents a nodekit catalog.\n"
        "\n"
        "This object describes notekit catalog entries.\n"
        "",                        /* tp_doc */
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
        (initproc)tp_init,        /* tp_init */
        0,                         /* tp_alloc */
        tp_new,                    /* tp_new */
    };

    return &catalogType;
}


void PyNodekitCatalog::tp_dealloc(Object* self)
{
    if (self->catalog)
    {
        self->catalog = 0;
    }

    Py_TYPE(self)->tp_free((PyObject*)self);
}


PyObject* PyNodekitCatalog::tp_new(PyTypeObject *type, PyObject* /*args*/, PyObject* /*kwds*/)
{
    Object *self = (Object *)type->tp_alloc(type, 0);
    if (self != NULL)
    {
        self->catalog = 0;
    }

    return (PyObject *)self;
}


int PyNodekitCatalog::tp_init(Object * /*self*/, PyObject * /*args*/, PyObject * /*kwds*/)
{
    Py_INCREF(Py_None);

    return 0;
}


PyObject *PyNodekitCatalog::tp_richcompare(Object *a, PyObject *b, int op)
{
    PyObject *returnObject = Py_NotImplemented;
    if (PyObject_TypeCheck(b, PyNodekitCatalog::getType()))
    {
        bool result = false;
        const SoNodekitCatalog *otherCatalog = ((Object*)b)->catalog;

        if (!otherCatalog || !a->catalog)
        {
            return NULL;
        }

        switch (op)
        {
        case Py_EQ:	result = a->catalog == otherCatalog ? true : false; break;
        case Py_NE:	result = a->catalog != otherCatalog ? true : false; break;
        default:
            // unsupported
            return NULL;
        }

        returnObject = result ? Py_True : Py_False;
    }

    Py_INCREF(returnObject);
    return  returnObject;
}


void PyNodekitCatalog::setInstance(const SoNodekitCatalog *catalog)
{
    Object *self = (Object *) this;
    self->catalog = catalog;
}


const SoNodekitCatalog *PyNodekitCatalog::getInstance(PyObject *self)
{
    if (PyObject_TypeCheck(self, getType()))
    {
        return ((Object*)self)->catalog;
    }

    return 0;
}


PyObject *PyNodekitCatalog::createWrapper(const SoNodekitCatalog *catalog)
{
    PyObject *obj = PyObject_CallObject((PyObject*)getType(), NULL);
    if (obj)
    {
        ((PyNodekitCatalog*)obj)->setInstance(catalog);
    }
    return obj;
}


Py_ssize_t PyNodekitCatalog::sq_length(Object *self)
{
    if (self->catalog)
    {
        return self->catalog->getNumEntries();
    }

    return 0;
}


int PyNodekitCatalog::sq_contains(Object *self, PyObject *item)
{
    int found = 0;
    char *name = 0;
    if (self->catalog && PyArg_ParseTuple(item, "s", &name))
    {
        found = self->catalog->getPartNumber(SbName(name)) != SO_CATALOG_NAME_NOT_FOUND;
    }

    return found;
}


PyObject *PyNodekitCatalog::sq_item(Object *self, Py_ssize_t idx)
{
    if (self->catalog)
    {
        if (idx < self->catalog->getNumEntries())
        {
            PyObject *obj = PyDict_New();
            PyDict_SetItem(obj, PyUnicode_FromString("Name"), PyUnicode_FromString(self->catalog->getName(idx).getString()));
            PyDict_SetItem(obj, PyUnicode_FromString("Type"), PyUnicode_FromString(self->catalog->getType(idx).getName().getString()));
            PyDict_SetItem(obj, PyUnicode_FromString("DefaultType"), PyUnicode_FromString(self->catalog->getDefaultType(idx).getName().getString()));
            PyDict_SetItem(obj, PyUnicode_FromString("NullByDefault"), PyBool_FromLong(self->catalog->isNullByDefault(idx)));
            PyDict_SetItem(obj, PyUnicode_FromString("Leaf"), PyBool_FromLong(self->catalog->isNullByDefault(idx)));
            PyDict_SetItem(obj, PyUnicode_FromString("ParentName"), PyUnicode_FromString(self->catalog->getParentName(idx).getString()));
            PyDict_SetItem(obj, PyUnicode_FromString("RightSiblingName"), PyUnicode_FromString(self->catalog->getRightSiblingName(idx).getString()));
            PyDict_SetItem(obj, PyUnicode_FromString("Public"), PyBool_FromLong(self->catalog->isPublic(idx)));

            return obj;
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
