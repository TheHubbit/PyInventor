/**
 * \file   
 * \brief      PyField class implementation.
 * \author     Thomas Moeller
 * \details
 *
 * Copyright (C) the PyInventor contributors. All rights reserved.
 * This file is part of PyInventor, distributed under the BSD 3-Clause
 * License. For full terms see the included COPYING file.
 */


#include <Inventor/fields/SoFields.h>
#include <Inventor/fields/SoFieldContainer.h>
#include <Inventor/nodekits/SoBaseKit.h>
#include "PyField.h"
#include "PyEngineOutput.h"

#pragma warning ( disable : 4127 ) // conditional expression is constant in Py_DECREF
#pragma warning ( disable : 4244 ) // possible loss of data when converting int to short in SbVec2s
#pragma warning ( disable : 4267 ) // possible loss of data in GET/SET macros

#include <numpy/arrayobject.h>
#include <numpy/ndarrayobject.h>




// macro for setting numerical multi-field (SoMField)
#define SOFIELD_SET_N(t, ct, nt, n, f, d) \
	if (f->isOfType(SoSF ## t ::getClassTypeId())) \
	{ \
		if (PyArrayObject *arr = (PyArrayObject*) PyArray_FROM_OTF(d, nt, NPY_ARRAY_IN_ARRAY | NPY_ARRAY_FORCECAST)) \
		{ \
			if (PyArray_SIZE(arr) == n) \
			{ \
				Sb ## t v; v.setValue((ct*) PyArray_BYTES(arr)); \
				((SoSF ## t *) f)->setValue(v); \
			} \
			Py_DECREF(arr); \
		} \
	} \
	else if (f->isOfType(SoMF ## t ::getClassTypeId())) \
	{ \
		if (PyArrayObject *arr = (PyArrayObject*) PyArray_FROM_OTF(d, nt, NPY_ARRAY_IN_ARRAY | NPY_ARRAY_FORCECAST)) \
		{ \
			if ((PyArray_SIZE(arr) % n) == 0) \
			{ \
				((SoMF ## t *) f)->setNum(PyArray_SIZE(arr) / n); \
				for (int i = 0; i < (PyArray_SIZE(arr) / n); ++i) \
				{ \
					Sb ## t v; v.setValue(((ct*) PyArray_BYTES(arr)) + (i * n)); \
					((SoMF ## t *) f)->set1Value(i, v); \
				} \
			} \
			Py_DECREF(arr); \
		} \
	}


// macro for setting numerical single-field (SoSField)
#define SOFIELD_SET(t, ct, nt, f, d) \
	if (f->isOfType(SoSF ## t ::getClassTypeId())) \
	{ \
		if (PyArrayObject *arr = (PyArrayObject*) PyArray_FROM_OTF(d, nt, NPY_ARRAY_IN_ARRAY | NPY_ARRAY_FORCECAST)) \
		{ \
			if (PyArray_SIZE(arr) == 1) \
				((SoSF ## t *) f)->setValue(*((ct*) PyArray_BYTES(arr))); \
			Py_DECREF(arr); \
		} \
	} \
	else if (f->isOfType(SoMF ## t ::getClassTypeId())) \
	{ \
		if (PyArrayObject *arr = (PyArrayObject*) PyArray_FROM_OTF(d, nt, NPY_ARRAY_IN_ARRAY | NPY_ARRAY_FORCECAST)) \
		{ \
			((SoMF ## t *) f)->setValues(0, PyArray_SIZE(arr), (ct*) PyArray_BYTES(arr)); \
			Py_DECREF(arr); \
		} \
	}

// macro for getting floating point single-field
#define SOFIELD_GETF(t, ct, nt, f) \
	if (f->isOfType(SoSF ## t ::getClassTypeId())) { return PyFloat_FromDouble(((SoSF ## t *) f)->getValue()); } \
	else if (f->isOfType(SoMF ## t ::getClassTypeId())) { return getPyObjectArrayFromData(nt, ((SoMF ## t *) f)->getValues(0), ((SoMF ## t *) f)->getNum()); }

// macro for getting integer single-field
#define SOFIELD_GETL(t, ct, nt, f) \
	if (f->isOfType(SoSF ## t ::getClassTypeId())) { return PyLong_FromLong(((SoSF ## t *) f)->getValue()); } \
	else if (f->isOfType(SoMF ## t ::getClassTypeId())) { return getPyObjectArrayFromData(nt, ((SoMF ## t *) f)->getValues(0), ((SoMF ## t *) f)->getNum()); }

// macro for getting numerical multi-field (SoMField)
#define SOFIELD_GET_N(t, ct, nt, n, f) \
	if (f->isOfType(SoSF ## t ::getClassTypeId())) { return getPyObjectArrayFromData(nt, ((SoSF ## t *) f)->getValue().getValue(), n); } \
	else if (f->isOfType(SoMF ## t ::getClassTypeId())) \
	{ \
		npy_intp dims[] = { ((SoMF ## t *) f)->getNum() , n }; \
		PyArrayObject *arr = (PyArrayObject*) PyArray_SimpleNew(2, dims, nt); \
		float *data = (float *) PyArray_BYTES(arr); \
		for (int i = 0; data && (i < dims[0]); ++i) \
			memcpy(data + (i * n), ((SoMF ## t *) f)->getValues(i)->getValue(), n * sizeof(float)); \
		return PyArray_Return(arr); \
	}



PyTypeObject *PyField::getType()
{
	static PyMethodDef methods[] = 
	{
		{"connect_from", (PyCFunction)connect_from, METH_VARARGS,
            "Connects this field as a slave to master.\n"
            "\n"
            "Args:\n"
            "    Field or engine output to connect to.\n"
        },
		{"append_connection", (PyCFunction)append_connection, METH_VARARGS,
            "Connects this field as a slave to master while keeping existing\n"
            "connections in place.\n"
            "\n"
            "Args:\n"
            "    Field or engine output to connect to.\n"
        },
        { "disconnect", (PyCFunction)disconnect, METH_VARARGS,
            "Disconnects connections from this field as a slave to master(s).\n"
             "\n"
             "Args:\n"
             "    Field or engine output. If none is provided then all connections\n"
             "    will be disconnected.\n"
        },
        {"is_connected", (PyCFunction)is_connected, METH_NOARGS,
            "Returns true if the field is connected to a master.\n"
            "\n"
            "Returns:\n"
            "    True if connection from field or engine is active, otherwise False.\n"
        },
        {"get_connected_engine", (PyCFunction)get_connected_engine, METH_NOARGS,
            "Returns engine output that this field is connected to.\n"
            "\n"
            "Returns:\n"
            "    Engine output that is connected to this field or None.\n"
        },
        {"get_connected_field", (PyCFunction)get_connected_field, METH_NOARGS,
            "Returns master field that this field is connected to.\n"
            "\n"
            "Returns:\n"
            "    Master field that is connected to this field or None.\n"
        },
        {"get_connections", (PyCFunction)get_connections, METH_NOARGS,
            "Returns a list of field connections.\n"
            "\n"
            "Returns:\n"
            "    List of fields that this fields is a slave of or None.\n"
        },        
        { "enable_connection", (PyCFunction)enable_connection, METH_VARARGS,
            "Enables or disables the connections to this field.\n"
            "\n"
            "Args:\n"
            "    True to enable and False to disable connection.\n"
        },
        { "is_connection_enabled", (PyCFunction)is_connection_enabled, METH_NOARGS,
            "Returns if connections to this field is considered active.\n"
            "\n"
            "Returns:\n"
            "    True if connection is active, otherwise False."
        },
        { "touch", (PyCFunction)touch, METH_NOARGS,
            "Notify the field as well as the field's owner that it has been changed.\n"
        },
        { "get_name", (PyCFunction)get_name, METH_NOARGS,
            "Returns the field name.\n"
            "\n"
            "Returns:\n"
            "    String containing the name under which the field is known in\n"
            "    its field container.\n"
        },
        { "get_type", (PyCFunction)get_type, METH_NOARGS,
            "Returns the type of the field.\n"
            "\n"
            "Returns:\n"
            "    Inventor field type as string."
        },
        { "get_container", (PyCFunction)get_container, METH_NOARGS,
            "Returns the container object of this field.\n"
            "\n"
            "Returns:\n"
            "    Instance of field container that the field is part of."
        },
        { "get_enums", (PyCFunction)get_enums, METH_NOARGS,
            "Returns the values that an enum or bitmask field understands.\n"
            "\n"
            "Returns:\n"
            "    List of strings that are valid values for this field."
        },
        {NULL}  /* Sentinel */
	};

	static PyTypeObject fieldType = 
	{
		PyVarObject_HEAD_INIT(NULL, 0)
		"Field",                  /* tp_name */
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
        (getattrofunc)tp_getattro,/* tp_getattro */
        (setattrofunc)tp_setattro,/* tp_setattro */
		0,                         /* tp_as_buffer */
		Py_TPFLAGS_DEFAULT |
		Py_TPFLAGS_BASETYPE,       /* tp_flags */
        "Represents a field.\n"
        "\n"
        "Field values can be accessed as attributes of a scene object. Use the field\n"
        "objects to create connections to other fields or engine outputs.\n", /* tp_doc */
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

	return &fieldType;
}


void PyField::tp_dealloc(Object* self)
{
	if (self->field)
	{
        self->field->getContainer()->unref();
	}

	Py_TYPE(self)->tp_free((PyObject*)self);
}


PyObject* PyField::tp_new(PyTypeObject *type, PyObject* /*args*/, PyObject* /*kwds*/)
{
	Object *self = (Object *)type->tp_alloc(type, 0);
	if (self != NULL) 
	{
		self->field = 0;
	}

	return (PyObject *) self;
}


int PyField::tp_init(Object *self, PyObject * /*args*/, PyObject * /*kwds*/)
{
	Py_INCREF(Py_None);

	return 0;
}


PyObject* PyField::tp_getattro(Object* self, PyObject *attrname)
{
    if (self->field && (PyUnicode_CompareWithASCIIString(attrname, "value") == 0))
    {
        return PyField::getFieldValue(self->field);
    }

    return PyObject_GenericGetAttr((PyObject*)self, attrname);
}


int PyField::tp_setattro(Object* self, PyObject *attrname, PyObject *value)
{
    if (self->field && (PyUnicode_CompareWithASCIIString(attrname, "value") == 0))
    {
        return PyField::setFieldValue(self->field, value);
    }

    return PyObject_GenericSetAttr((PyObject*)self, attrname, value);
}


PyObject *PyField::tp_richcompare(Object *a, PyObject *b, int op)
{
    PyObject *returnObject = Py_NotImplemented;
    if (PyObject_TypeCheck(b, PyField::getType()))
    {
        bool result = false;
        void *otherField = ((Object*)b)->field;

        switch (op)
        {
        case Py_LT:	result = a->field < otherField; break;
        case Py_LE:	result = a->field <= otherField; break;
        case Py_EQ:	result = a->field == otherField; break;
        case Py_NE:	result = a->field != otherField; break;
        case Py_GT:	result = a->field > otherField; break;
        case Py_GE:	result = a->field >= otherField; break;
        }

        returnObject = result ? Py_True : Py_False;
    }

    Py_INCREF(returnObject);
    return  returnObject;
}


void PyField::setInstance(SoField *field)
{
    Object *self = (Object *) this;
    if (self->field)
    {
        self->field->getContainer()->unref();
    }
    self->field = field;
    if (self->field)
    {
        self->field->getContainer()->ref();
    }
}


SoField *PyField::getInstance(PyObject *self)
{
    if (PyObject_TypeCheck(self, getType()))
    {
        return ((Object*)self)->field;
    }

    return 0;
}


bool PyField::initNumpy()
{
    if (PyArray_API == NULL)
    {
        import_array1(false);
    }

    return true;
}


bool PyField::getFloatsFromPyObject(PyObject *obj, int size, float *value_out)
{
    initNumpy();

    if (obj)
    {
        if (PyArrayObject *arr = (PyArrayObject*)PyArray_FROM_OTF(obj, NPY_FLOAT32, NPY_ARRAY_IN_ARRAY | NPY_ARRAY_FORCECAST))
        {
            if (PyArray_SIZE(arr) == size)
            {
                memcpy(value_out, PyArray_BYTES(arr), sizeof(float) * size);
                Py_DECREF(arr);
                return true;
            }
            Py_DECREF(arr);
        }
    }

    return false;
}


PyObject *PyField::getPyObjectArrayFromData(int type, const void* data, int dim1, int dim2, int dim3)
{
    initNumpy();

    npy_intp dims[] = { dim1, dim2, dim3 };
    int num = 1;
    if (dim2 > 0) num++;
    if (dim3 > 0) num++;

    PyArrayObject *arr = (PyArrayObject*)PyArray_SimpleNew(num, dims, type);
    if (arr)
    {
        memcpy(PyArray_DATA(arr), data, PyArray_NBYTES(arr));
        return PyArray_Return(arr);
    }

    return Py_None;
}


PyObject *PyField::getFieldValue(SoField *field)
{
    initNumpy();
    PyObject *result = NULL;

    if (field->isOfType(SoSFNode::getClassTypeId()))
    {
        SoSFNode *nodeField = (SoSFNode*)field;
        SoNode *node = nodeField->getValue();
        if (node)
        {
            PyObject *found = PySceneObject::createWrapper(node);
            if (found)
            {
                return found;
            }
        }
        Py_INCREF(Py_None);
        return Py_None;
    }
    else if (field->isOfType(SoMFNode::getClassTypeId()))
    {
        SoMFNode *nodeField = (SoMFNode*)field;
        PyObject *result = PyList_New(nodeField->getNum());
        for (Py_ssize_t i = 0; i < nodeField->getNum(); ++i)
        {
            SoNode *node = (SoNode*)*nodeField->getValues(i);
            PyList_SetItem(result, i, PySceneObject::createWrapper(node));
        }

        return result;
    }
    else if (field->isOfType(SoSFMatrix::getClassTypeId()))
    {
        // special case for single matrix: return as [4][4] rather than [16]
        return getPyObjectArrayFromData(NPY_FLOAT32, ((SoSFMatrix *)field)->getValue().getValue(), 4, 4);
    }
    else if (field->isOfType(SoSFImage::getClassTypeId()))
    {
        SbVec2s size;
        int nc;
        const unsigned char *pixel = ((SoSFImage*)field)->getValue(size, nc);
        if (pixel)
        {
            PyObject *obj = PyTuple_New(4);
            PyTuple_SetItem(obj, 0, PyLong_FromLong(size[0]));
            PyTuple_SetItem(obj, 1, PyLong_FromLong(size[1]));
            PyTuple_SetItem(obj, 2, PyLong_FromLong(nc));
            PyTuple_SetItem(obj, 3, getPyObjectArrayFromData(NPY_UBYTE, pixel, size[0] * size[1] * nc));

            return obj;
        }

        Py_INCREF(Py_None);
        return Py_None;
    }
    else if (field->isOfType(SoSFPlane::getClassTypeId()))
    {
        SbPlane p = ((SoSFPlane*)field)->getValue();
        SbVec4f v;
        v[0] = p.getNormal()[0]; v[1] = p.getNormal()[1]; v[2] = p.getNormal()[2];
        v[3] = p.getDistanceFromOrigin();

        return getPyObjectArrayFromData(NPY_FLOAT32, v.getValue(), 4);
    }
    else if (field->isOfType(SoMFPlane::getClassTypeId()))
    {
        npy_intp dims[] = { ((SoMFPlane*)field)->getNum() , 4 };
        PyArrayObject *arr = (PyArrayObject*)PyArray_SimpleNew(2, dims, NPY_FLOAT32);
        float *data = (float *)PyArray_GETPTR1(arr, 0);
        for (int i = 0; data && (i < dims[0]); ++i)
        {
            SbPlane p = *((SoMFPlane *)field)->getValues(i);
            data[i * 4 + 0] = p.getNormal()[0];
            data[i * 4 + 1] = p.getNormal()[1];
            data[i * 4 + 2] = p.getNormal()[2];
            data[i * 4 + 3] = p.getDistanceFromOrigin();
        }
        return PyArray_Return(arr);
    }
    else if (field->isOfType(SoSFString::getClassTypeId()))
    {
        SbString s = ((SoSFString*)field)->getValue();
#ifdef TGS_VERSION
        result = PyUnicode_FromUnicode(s.toWideChar(), s.getLength());
#else
        result = PyUnicode_FromString(s.getString());
#endif
        return result;
    }
    else if (field->isOfType(SoMFString::getClassTypeId()))
    {
        result = PyList_New(((SoMField*)field)->getNum());
        for (int i = 0; i < ((SoMField*)field)->getNum(); ++i)
        {
            SbString s = *(((SoMFString*)field)->getValues(i));
            PyList_SetItem(result, i,
#ifdef TGS_VERSION
                PyUnicode_FromUnicode(s.toWideChar(), s.getLength())
#else
                PyUnicode_FromString(s.getString())
#endif
            );
        }
        return result;
    }

    SOFIELD_GETF(Float, float, NPY_FLOAT32, field);
    SOFIELD_GETF(Double, float, NPY_FLOAT64, field);
    SOFIELD_GETL(Int32, int, NPY_INT32, field);
    SOFIELD_GETL(UInt32, unsigned int, NPY_UINT32, field);
    SOFIELD_GETL(Short, short, NPY_INT16, field);
    SOFIELD_GETL(UShort, unsigned short, NPY_UINT16, field);
    SOFIELD_GETL(Bool, int, NPY_INT32, field);

    SOFIELD_GET_N(Vec2f, float, NPY_FLOAT32, 2, field);
    SOFIELD_GET_N(Vec3f, float, NPY_FLOAT32, 3, field);
    SOFIELD_GET_N(Vec4f, float, NPY_FLOAT32, 4, field);
    SOFIELD_GET_N(Color, float, NPY_FLOAT32, 3, field);
    SOFIELD_GET_N(Rotation, float, NPY_FLOAT32, 4, field);
    SOFIELD_GET_N(Matrix, float, NPY_FLOAT32, 16, field);

    if (field->isOfType(SoMField::getClassTypeId()))
    {
        // generic string array
        result = PyList_New(((SoMField*)field)->getNum());
        for (int i = 0; i < ((SoMField*)field)->getNum(); ++i)
        {
            SbString s;
            ((SoMField*)field)->get1(i, s);
            PyList_SetItem(result, i, PyUnicode_FromString(s.getString()));
        }
        return result;
    }

    // generic string based fallback
    SbString s;
    field->get(s);
    result = PyUnicode_FromString(s.getString());

    return result;
}


int PyField::setFieldValue(SoField *field, PyObject *value)
{
    initNumpy();
    int result = 0;

    if (field->isOfType(SoSFNode::getClassTypeId()))
    {
        SoSFNode *nodeField = (SoSFNode*)field;
        if (PyNode_Check(value))
        {
            PySceneObject::Object *child = (PySceneObject::Object *)value;
            if (child->inventorObject && child->inventorObject->isOfType(SoNode::getClassTypeId()))
            {
                if (field->getContainer() && field->getContainer()->isOfType(SoBaseKit::getClassTypeId()))
                {
                    SoBaseKit *baseKit = (SoBaseKit *)field->getContainer();
                    SbName fieldName;
                    if (baseKit->getFieldName(field, fieldName))
                    {
                        if (baseKit->getNodekitCatalog()->getPartNumber(fieldName) >= 0)
                        {
                            // update part of a node kit
                            baseKit->setPart(fieldName, (SoNode*)child->inventorObject);
                            return result;
                        }
                    }
                }

                nodeField->setValue((SoNode*)child->inventorObject);
            }
        }
        else
        {
            nodeField->setValue(0);
        }
    }
    else if (field->isOfType(SoMFNode::getClassTypeId()))
    {
        SoMFNode *nodeField = (SoMFNode*)field;
        if (PyNode_Check(value))
        {
            PySceneObject::Object *child = (PySceneObject::Object *)value;
            if (child->inventorObject && child->inventorObject->isOfType(SoNode::getClassTypeId()))
            {
                nodeField->setValue((SoNode*)child->inventorObject);
            }
        }
        else
        {
            PyObject *seq = PySequence_Fast(value, "expected a sequence");
            size_t n = PySequence_Size(seq);
            nodeField->setNum(n);
            for (size_t i = 0; i < n; ++i)
            {
                PyObject *seqItem = PySequence_GetItem(seq, i);
                if (seqItem && PyNode_Check(seqItem))
                {
                    PySceneObject::Object *child = (PySceneObject::Object *)seqItem;
                    nodeField->set1Value(i, (SoNode*)child->inventorObject);
                }
            }

            Py_XDECREF(seq);
        }
    }
    else if (field->isOfType(SoSFString::getClassTypeId()))
    {
        PyObject *str = PyObject_Str(value);
        if (str)
        {
            Py_ssize_t len;
#ifdef TGS_VERSION
            ((SoSFString*)field)->setValue(PyUnicode_AsWideCharString(str, &len));
#else
            ((SoSFString*)field)->setValue(PyUnicode_AsUTF8AndSize(str, &len));
#endif
            Py_DECREF(str);
        }
    }
    else if (field->isOfType(SoMFString::getClassTypeId()))
    {
        if (!PyUnicode_Check(value) && PySequence_Check(value))
        {
            PyObject *seq = PySequence_Fast(value, "expected a sequence");
            size_t n = PySequence_Size(seq);
            ((SoMFString*)field)->setNum(n);

            for (size_t i = 0; i < n; ++i)
            {
                PyObject *item = PySequence_GetItem(seq, i);
                if (item)
                {
                    PyObject *str = PyObject_Str(item);
                    if (str)
                    {
                        Py_ssize_t len;
#ifdef TGS_VERSION
                        ((SoMFString*)field)->set1Value(i, PyUnicode_AsWideCharString(str, &len));
#else
                        ((SoMFString*)field)->set1Value(i, PyUnicode_AsUTF8AndSize(str, &len));
#endif
                        Py_DECREF(str);
                    }
                }
            }

            Py_DECREF(seq);
        }
        else
        {
            PyObject *str = PyObject_Str(value);
            if (str)
            {
                Py_ssize_t len;
#ifdef TGS_VERSION
                ((SoMFString*)field)->setValue(PyUnicode_AsWideCharString(str, &len));
#else
                ((SoMFString*)field)->setValue(PyUnicode_AsUTF8AndSize(str, &len));
#endif
                Py_DECREF(str);
            }
        }
    }
    else if (field->isOfType(SoSFTrigger::getClassTypeId()))
    {
        field->touch();
    }
    else if (!PyUnicode_Check(value))
    {
        if (field->isOfType(SoSFImage::getClassTypeId()))
        {
            PyObject *pixelObj = 0;
            int width = 0, height = 0, nc = 0;
            if (PyArg_ParseTuple(value, "iii|O", &width, &height, &nc, &pixelObj))
            {
                if ((width * height * nc) > 0)
                {
                    if (PyBytes_Check(pixelObj))
                    {
                        size_t n = PyBytes_Size(pixelObj);
                        if ((width * height * nc) <= n)
                        {
                            const unsigned char *data = (const unsigned char *)PyBytes_AsString(pixelObj);
                            ((SoSFImage*)field)->setValue(SbVec2s(width, height), nc, data);
                        }
                    }
                    else if (PyByteArray_Check(pixelObj))
                    {
                        size_t n = PyByteArray_Size(pixelObj);
                        if ((width * height * nc) <= n)
                        {
                            const unsigned char *data = (const unsigned char *)PyByteArray_AsString(pixelObj);
                            ((SoSFImage*)field)->setValue(SbVec2s(width, height), nc, data);
                        }
                    }
                    else
                    {
                        PyArrayObject *arr = (PyArrayObject*)PyArray_FROM_OTF(pixelObj, NPY_UBYTE, NPY_ARRAY_IN_ARRAY | NPY_ARRAY_FORCECAST);
                        if (arr)
                        {
                            size_t n = PyArray_SIZE(arr);
                            if ((width * height * nc) <= n)
                            {
                                const unsigned char *data = (const unsigned char *)PyArray_GETPTR1(arr, 0);
                                ((SoSFImage*)field)->setValue(SbVec2s(width, height), nc, data);
                            }

                            Py_DECREF(arr);
                        }
                    }
                }
                else
                {
                    ((SoSFImage*)field)->setValue(SbVec2s(0, 0), 0, 0);
                }
            }
        }
        else if (field->isOfType(SoSFPlane::getClassTypeId()) || field->isOfType(SoMFPlane::getClassTypeId()))
        {
            PyArrayObject *arr = (PyArrayObject*)PyArray_FROM_OTF(value, NPY_FLOAT32, NPY_ARRAY_IN_ARRAY | NPY_ARRAY_FORCECAST);
            if (arr)
            {
                size_t n = PyArray_SIZE(arr);
                float *data = (float *)PyArray_GETPTR1(arr, 0);

                if (field->isOfType(SoSFPlane::getClassTypeId()))
                {
                    if (n == 4)
                    {
                        ((SoSFPlane*)field)->setValue(SbPlane(SbVec3f(data[0], data[1], data[2]), data[3]));
                    }
                }
                else if (field->isOfType(SoMFPlane::getClassTypeId()))
                {
                    if ((n % 4) == 0)
                    {
                        ((SoMFPlane*)field)->setNum(n / 4);
                        for (int i = 0; i < (n / 4); ++i)
                        {
                            float *p = data + (i * 3);
                            ((SoMFPlane*)field)->set1Value(i, SbPlane(SbVec3f(p[0], p[1], p[2]), p[3]));
                        }
                    }
                }
                Py_DECREF(arr);
            }
        }
        else if (field->isOfType(SoSFRotation::getClassTypeId()))
        {
            bool valueWasSet = false;
            PyObject *axis = 0, *fromVec = 0, *toVec = 0;
            float angle = 0.;

            if (PyArg_ParseTuple(value, "Of", &axis, &angle))
            {
                // axis plus angle
                float axisValue[3];
                if (getFloatsFromPyObject(axis, 3, axisValue))
                {
                    ((SoSFRotation*)field)->setValue(SbVec3f(axisValue), angle);
                    valueWasSet = true;
                }
            }
            else if (PyArg_ParseTuple(value, "OO", &fromVec, &toVec))
            {
                // from & to vectors
                float fromValue[3], toValue[3];
                if (getFloatsFromPyObject(fromVec, 3, fromValue) && getFloatsFromPyObject(toVec, 3, toValue))
                {
                    ((SoSFRotation*)field)->setValue(SbRotation(SbVec3f(fromValue), SbVec3f(toValue)));
                    valueWasSet = true;
                }
            }

            if (!valueWasSet)
            {
                float value[16];
                if (getFloatsFromPyObject(axis, 16, value))
                {
                    // matrix
                    SbMatrix m;
                    m.setValue(value);
                    ((SoSFRotation*)field)->setValue(SbRotation(m));
                }
                else if (getFloatsFromPyObject(axis, 4, value))
                {
                    // quaternion
                    ((SoSFRotation*)field)->setValue(value);
                }
            }
        }
        else SOFIELD_SET(Float, float, NPY_FLOAT32, field, value)
        else SOFIELD_SET(Double, double, NPY_FLOAT64, field, value)
        else SOFIELD_SET(Int32, int, NPY_INT32, field, value)
        else SOFIELD_SET(UInt32, unsigned int, NPY_UINT32, field, value)
        else SOFIELD_SET(Short, short, NPY_INT16, field, value)
        else SOFIELD_SET(UShort, unsigned short, NPY_UINT16, field, value)
        else SOFIELD_SET(Bool, int, NPY_INT32, field, value)
        else SOFIELD_SET_N(Vec2f, float, NPY_FLOAT32, 2, field, value)
        else SOFIELD_SET_N(Vec3f, float, NPY_FLOAT32, 3, field, value)
        else SOFIELD_SET_N(Vec4f, float, NPY_FLOAT32, 4, field, value)
        else SOFIELD_SET_N(Color, float, NPY_FLOAT32, 3, field, value)
        else SOFIELD_SET_N(Rotation, float, NPY_FLOAT32, 4, field, value)
        else SOFIELD_SET_N(Matrix, float, NPY_FLOAT32, 16, field, value)
    }
    else
    {
        // generic string API
        if (!PyUnicode_Check(value) && PySequence_Check(value) && field->isOfType(SoMField::getClassTypeId()))
        {
            PyObject *seq = PySequence_Fast(value, "expected a sequence");
            size_t n = PySequence_Size(seq);
            ((SoMFString*)field)->setNum(n);

            for (size_t i = 0; i < n; ++i)
            {
                PyObject *item = PySequence_GetItem(seq, i);
                if (item)
                {
                    PyObject *str = PyObject_Str(item);
                    if (str)
                    {
                        Py_ssize_t len;
                        ((SoMField*)field)->set1(i, PyUnicode_AsUTF8AndSize(str, &len));
                        Py_DECREF(str);
                    }
                }
            }

            Py_DECREF(seq);
        }
        else
        {
            PyObject *str = PyObject_Str(value);
            if (str)
            {
                Py_ssize_t len;
                field->set(PyUnicode_AsUTF8AndSize(str, &len));
                Py_DECREF(str);
            }
        }
    }

    return result;
}


PyObject* PyField::connect_from(Object *self, PyObject *args)
{
    long connected = 0;
    PyObject *master = NULL;
    if (self->field && PyArg_ParseTuple(args, "O", &master))
    {
        if (master)
        {
            if (PyObject_TypeCheck(master, PyField::getType()))
            {
                connected = self->field->connectFrom(((Object*)master)->field);
            }
            else if (PyObject_TypeCheck(master, PyEngineOutput::getType()))
            {
                connected = self->field->connectFrom(PyEngineOutput::getInstance(master));
            }
        }
    }

    return PyBool_FromLong(connected);
}


PyObject* PyField::append_connection(Object *self, PyObject *args)
{
    long connected = 0;
    PyObject *master = NULL;
    if (self->field && PyArg_ParseTuple(args, "O", &master))
    {
        if (master)
        {
            if (PyObject_TypeCheck(master, PyField::getType()))
            {
                connected = self->field->appendConnection(((Object*)master)->field);
            }
            else if (PyObject_TypeCheck(master, PyEngineOutput::getType()))
            {
                connected = self->field->appendConnection(PyEngineOutput::getInstance(master));
            }
        }
    }

    return PyBool_FromLong(connected);
}


PyObject* PyField::disconnect(Object *self, PyObject *args)
{
    PyObject *master = NULL;
    if (self->field && PyArg_ParseTuple(args, "|O", &master))
    {
        if (master)
        {
            if (PyObject_TypeCheck(master, PyField::getType()))
            {
                self->field->disconnect(((Object*)master)->field);
            }
            else if (PyObject_TypeCheck(master, PyEngineOutput::getType()))
            {
                self->field->disconnect(PyEngineOutput::getInstance(master));
            }
        }
        else
        {
            self->field->disconnect();
        }
    }

    Py_INCREF(Py_None);
    return Py_None;
}


PyObject* PyField::is_connected(Object *self)
{
    long connected = 0;
    if (self->field)
    {
        connected = self->field->isConnected();
    }

    return PyBool_FromLong(connected);
}


PyObject* PyField::get_connected_engine(Object *self)
{
    if (self->field)
    {
        SoEngineOutput *output = 0;
        if (self->field->getConnectedEngine(output))
        {
            PyObject *outputWrapper = PyObject_CallObject((PyObject*)PyEngineOutput::getType(), NULL);
            ((PyEngineOutput*)outputWrapper)->setInstance(output);
            return outputWrapper;
        }
    }

    Py_INCREF(Py_None);
    return Py_None;
}


PyObject* PyField::get_connected_field(Object *self)
{
    if (self->field)
    {
        SoField *field = 0;
        if (self->field->getConnectedField(field))
        {
            PyObject *outputWrapper = PyObject_CallObject((PyObject*)getType(), NULL);
            ((PyField*)outputWrapper)->setInstance(field);
            return outputWrapper;
        }
    }

    Py_INCREF(Py_None);
    return Py_None;
}


PyObject* PyField::get_connections(Object *self)
{
    if (self->field)
    {
        SoFieldList fieldList;
        int numFields = self->field->getConnections(fieldList);
        PyObject *result = PyList_New(numFields);
        for (int i = 0; i < numFields; ++i)
        {
            PyObject *fieldWrapper = PyObject_CallObject((PyObject*)PyField::getType(), NULL);
            ((PyField*)fieldWrapper)->setInstance(fieldList[i]);
            PyList_SetItem(result, i, fieldWrapper);
        }
        return result;
    }

    Py_INCREF(Py_None);
    return Py_None;
}


PyObject* PyField::enable_connection(Object *self, PyObject *args)
{
    int enable = 0;
    if (self->field && PyArg_ParseTuple(args, "p", &enable))
    {
        self->field->enableConnection(enable);
    }

    Py_INCREF(Py_None);
    return Py_None;
}


PyObject* PyField::is_connection_enabled(Object *self)
{
    long enabled = 0;
    if (self->field)
    {
        enabled = self->field->isConnectionEnabled();
    }

    return PyBool_FromLong(enabled);
}


PyObject* PyField::touch(Object *self)
{
    if (self->field)
    {
        self->field->touch();
    }

    Py_INCREF(Py_None);
    return Py_None;
}


PyObject* PyField::get_name(Object *self)
{
    if (self->field && self->field->getContainer())
    {
        SbName fieldName("");
        self->field->getContainer()->getFieldName(self->field, fieldName);
        return PyUnicode_FromString(fieldName.getString());
    }

    Py_INCREF(Py_None);
    return Py_None;
}


PyObject* PyField::get_type(Object *self)
{
    if (self->field)
    {
        return PyUnicode_FromString(self->field->getTypeId().getName().getString());
    }

    Py_INCREF(Py_None);
    return Py_None;
}


PyObject* PyField::get_container(Object *self)
{
    if (self->field && self->field->getContainer())
    {
        SoFieldContainer *obj = self->field->getContainer();
        return PySceneObject::createWrapper(obj);
    }

    Py_INCREF(Py_None);
    return Py_None;
}


PyObject* PyField::get_enums(Object *self)
{
    if (self->field)
    {
        if (self->field->isOfType(SoSFEnum::getClassTypeId()))
        {
            SoSFEnum *enumField = (SoSFEnum*)self->field;
            PyObject *result = PyList_New(enumField->getNumEnums());
            for (Py_ssize_t i = 0; i < enumField->getNumEnums(); ++i)
            {
                SbName name;
                enumField->getEnum(i, name);
                PyList_SetItem(result, i, PyUnicode_FromString(name.getString()));
            }
            return result;
        }
        else if (self->field->isOfType(SoMFEnum::getClassTypeId()))
        {
            SoMFEnum *enumField = (SoMFEnum*)self->field;
            PyObject *result = PyList_New(enumField->getNumEnums());
            for (Py_ssize_t i = 0; i < enumField->getNumEnums(); ++i)
            {
                SbName name;
                enumField->getEnum(i, name);
                PyList_SetItem(result, i, PyUnicode_FromString(name.getString()));
            }
            return result;
        }
    }

    Py_INCREF(Py_None);
    return Py_None;
}
