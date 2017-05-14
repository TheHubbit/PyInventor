/**
 * \file   
 * \brief      PyEngineOutput class implementation.
 * \author     Thomas Moeller
 * \details
 *
 * Copyright (C) the PyInventor contributors. All rights reserved.
 * This file is part of PyInventor, distributed under the BSD 3-Clause
 * License. For full terms see the included COPYING file.
 */


#include <Inventor/engines/SoEngine.h>
#include <Inventor/SbName.h>
#include "PyEngineOutput.h"

#pragma warning ( disable : 4127 ) // conditional expression is constant in Py_DECREF
#pragma warning ( disable : 4244 ) // possible loss of data when converting int to short in SbVec2s
#pragma warning ( disable : 4267 ) // possible loss of data in GET/SET macros


PyTypeObject *PyEngineOutput::getType()
{
	static PyMethodDef methods[] = 
	{
        { "get_name", (PyCFunction)get_name, METH_NOARGS,
            "Returns the engine name.\n"
            "\n"
            "Returns:\n"
            "    String containing the name under which the output is known in\n"
            "    the engine.\n"
        },
        { "get_type", (PyCFunction)get_type, METH_NOARGS,
            "Returns the type of the engine output.\n"
            "\n"
            "Returns:\n"
            "    Engine connection type as string."
        },
        { "get_container", (PyCFunction)get_container, METH_NOARGS,
            "Returns the engine of this output.\n"
            "\n"
            "Returns:\n"
            "    Engine instance that this output is a part of."
        },
        { "enable", (PyCFunction)enable, METH_VARARGS,
            "Enables or disables the connections from this output.\n"
            "\n"
            "Args:\n"
            "    True to enable and False to disable connection.\n"
        },
        { "is_enabled", (PyCFunction)is_enabled, METH_NOARGS,
            "Returns if connections from this output are enabled.\n"
            "\n"
            "Returns:\n"
            "    True if connection is active, otherwise False."
        },
        {NULL}  /* Sentinel */
	};

	static PyTypeObject engineOutputType = 
	{
		PyVarObject_HEAD_INIT(NULL, 0)
		"EngineOutput",            /* tp_name */
		sizeof(Object),            /* tp_basicsize */
		0,                         /* tp_itemsize */
		(destructor) tp_dealloc,   /* tp_dealloc */
		0,                         /* tp_print */
		0,                         /* tp_getattr */
		0,                         /* tp_setattr */
		0,                         /* tp_reserved */
		0,                         /* tp_repr */
		0,                         /* tp_as_number */
		0,                         /* tp_as_sequence */
		0,                         /* tp_as_mapping */
		0,                         /* tp_hash  */
		0,                         /* tp_call */
		0,                         /* tp_str */
        0,                         /* tp_getattro */
        0,                         /* tp_setattro */
		0,                         /* tp_as_buffer */
		Py_TPFLAGS_DEFAULT |
		Py_TPFLAGS_BASETYPE,       /* tp_flags */
        "Represents an engine output.\n"
        "\n"
        "Use this object type to create connections to fields.\n", /* tp_doc */
		0,                         /* tp_traverse */
		0,                         /* tp_clear */
        (richcmpfunc)tp_richcompare, /* tp_richcompare */
        0,                         /* tp_weaklistoffset */
		0,                         /* tp_iter */
		0,                         /* tp_iternext */
		methods,                   /* tp_methods */
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

	return &engineOutputType;
}


void PyEngineOutput::tp_dealloc(Object* self)
{
	if (self->output)
	{
        self->output->getContainer()->unref();
	}

	Py_TYPE(self)->tp_free((PyObject*)self);
}


PyObject* PyEngineOutput::tp_new(PyTypeObject *type, PyObject* /*args*/, PyObject* /*kwds*/)
{
	Object *self = (Object *)type->tp_alloc(type, 0);
	if (self != NULL) 
	{
		self->output = 0;
	}

	return (PyObject *) self;
}


int PyEngineOutput::tp_init(Object * /*self*/, PyObject * /*args*/, PyObject * /*kwds*/)
{
	Py_INCREF(Py_None);

	return 0;
}


PyObject *PyEngineOutput::tp_richcompare(Object *a, PyObject *b, int op)
{
    PyObject *returnObject = Py_NotImplemented;
    if (PyObject_TypeCheck(b, PyEngineOutput::getType()))
    {
        bool result = false;
        void *otherOutput = ((Object*)b)->output;

        switch (op)
        {
        case Py_LT:	result = a->output < otherOutput; break;
        case Py_LE:	result = a->output <= otherOutput; break;
        case Py_EQ:	result = a->output == otherOutput; break;
        case Py_NE:	result = a->output != otherOutput; break;
        case Py_GT:	result = a->output > otherOutput; break;
        case Py_GE:	result = a->output >= otherOutput; break;
        }

        returnObject = result ? Py_True : Py_False;
    }

    Py_INCREF(returnObject);
    return  returnObject;
}


void PyEngineOutput::setInstance(SoEngineOutput *field)
{
    Object *self = (Object *) this;
    if (self->output)
    {
        self->output->getContainer()->unref();
    }
    self->output = field;
    if (self->output)
    {
        self->output->getContainer()->ref();
    }
}


SoEngineOutput *PyEngineOutput::getInstance(PyObject *self)
{
    if (PyObject_TypeCheck(self, getType()))
    {
        return ((Object*)self)->output;
    }
    
    return 0;
}


PyObject* PyEngineOutput::get_name(Object *self)
{
    if (self->output && self->output->getContainer())
    {
        SbName engineName("");
        ((SoEngine*)self->output->getContainer())->getOutputName(self->output, engineName);
        return PyUnicode_FromString(engineName.getString());
    }

    Py_INCREF(Py_None);
    return Py_None;
}


PyObject* PyEngineOutput::get_type(Object *self)
{
    if (self->output)
    {
        return PyUnicode_FromString(self->output->getConnectionType().getName().getString());
    }

    Py_INCREF(Py_None);
    return Py_None;
}


PyObject* PyEngineOutput::get_container(Object *self)
{
    if (self->output && self->output->getContainer())
    {
        SoFieldContainer *obj = self->output->getContainer();
        return PySceneObject::createWrapper(obj);
    }

    Py_INCREF(Py_None);
    return Py_None;
}


PyObject* PyEngineOutput::enable(Object *self, PyObject *args)
{
    int enable = 0;
    if (self->output && PyArg_ParseTuple(args, "p", &enable))
    {
        self->output->enable(enable);
    }

    Py_INCREF(Py_None);
    return Py_None;
}


PyObject* PyEngineOutput::is_enabled(Object *self)
{
    long enabled = 0;
    if (self->output)
    {
        enabled = self->output->isEnabled();
    }

    return PyBool_FromLong(enabled);
}
