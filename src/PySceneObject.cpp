/**
 * \file   
 * \brief      PySceneObject class implementation.
 * \author     Thomas Moeller
 * \details
 *
 * Copyright (C) the PyInventor contributors. All rights reserved.
 * This file is part of PyInventor, distributed under the BSD 3-Clause
 * License. For full terms see the included COPYING file.
 */


#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodekits/SoBaseKit.h>
#include <Inventor/engines/SoEngine.h>
#include <Inventor/fields/SoFields.h>
#include <Inventor/actions/SoSearchAction.h>
#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/SoDB.h>
#include <Inventor/SoInteraction.h>
#include <map>
#include <string>

#include "PySceneObject.h"

#pragma warning ( disable : 4127 ) // conditional expression is constant in Py_DECREF
#pragma warning ( disable : 4244 ) // possible loss of data in GET/SET macros
#pragma warning ( disable : 4267 ) // possible loss of data in GET/SET macros

#include <numpy/arrayobject.h>
#include <numpy/libnumarray.h>


typedef std::map<std::string, PyTypeObject> WrapperTypes;


// by defining PRESODBINIT an external initialization function can configured
#ifdef PRESODBINIT
#ifdef _WIN32
void __declspec( dllimport ) PRESODBINIT();
#endif
#else
#define PRESODBINIT() /* unused */
#endif


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
	if (f->isOfType(SoSF ## t ::getClassTypeId())) \
		return PyFloat_FromDouble(((SoSF ## t *) f)->getValue()); \
	else if (f->isOfType(SoMF ## t ::getClassTypeId())) \
	{ \
		npy_intp dims[] = { ((SoMF ## t *) f)->getNum() }; \
		PyArrayObject *arr = (PyArrayObject*) PyArray_SimpleNewFromData(1, dims, nt, (void*) ((SoMF ## t *) f)->getValues(0)); \
		return PyArray_Return(arr); \
	}

// macro for getting integer single-field
#define SOFIELD_GETL(t, ct, nt, f) \
	if (f->isOfType(SoSF ## t ::getClassTypeId())) \
		return PyLong_FromLong(((SoSF ## t *) f)->getValue()); \
	else if (f->isOfType(SoMF ## t ::getClassTypeId())) \
	{ \
		npy_intp dims[] = { ((SoMF ## t *) f)->getNum() }; \
		PyArrayObject *arr = (PyArrayObject*) PyArray_SimpleNewFromData(1, dims, nt, (void*) ((SoMF ## t *) f)->getValues(0)); \
		return PyArray_Return(arr); \
	}

// macro for getting numerical multi-field (SoMField)
#define SOFIELD_GET_N(t, ct, nt, n, f) \
	if (f->isOfType(SoSF ## t ::getClassTypeId())) \
	{ \
		npy_intp dims[] = { n }; \
		PyArrayObject *arr = (PyArrayObject*) PyArray_SimpleNewFromData(1, dims, nt, (void*) ((SoSF ## t *) f)->getValue().getValue()); \
		return PyArray_Return(arr); \
	} \
	else if (f->isOfType(SoMF ## t ::getClassTypeId())) \
	{ \
		npy_intp dims[] = { ((SoMF ## t *) f)->getNum() , n }; \
		PyArrayObject *arr = (PyArrayObject*) PyArray_SimpleNew(2, dims, nt); \
		float *data = (float *) PyArray_BYTES(arr); \
		for (int i = 0; data && (i < dims[0]); ++i) \
			memcpy(data + (i * n), ((SoMF ## t *) f)->getValues(i)->getValue(), n * sizeof(float)); \
		return PyArray_Return(arr); \
	}


PyTypeObject *PySceneObject::getFieldContainerType()
{
	static PyMethodDef methods[] = 
	{
		{"setname", (PyCFunction) setname, METH_VARARGS,
            "Sets the instance name of a scene object.\n"
            "\n"
            "Args:\n"
            "    Name for scene object instance.\n"
        },
		{"getname", (PyCFunction) getname, METH_NOARGS,
            "Returns the instance name of a scene object.\n"
            "\n"
            "Returns:\n"
            "    String containing scene object name.\n"
        },
		{"sotype", (PyCFunction) sotype, METH_NOARGS,
            "Return the type name of a scene object.\n"
            "\n"
            "Returns:\n"
            "    String containing scene object type.\n"
        },
		{"touch", (PyCFunction) touch, METH_NOARGS,
            "Marks a scene object as modified."
        },
		{"enable_notify", (PyCFunction) enable_notify, METH_VARARGS,
            "Enables or disables change notifications for a scene object.\n"
            "\n"
            "Args:\n"
            "    Boolean value indicating if notifications are enabled (True)\n"
            "    or not (False).\n"
        },
		{"connect", (PyCFunction) connect, METH_VARARGS,
            "Connects a field from another field or engine output.\n"
            "\n"
            "Args:\n"
            "    toField: Field name of this instance that will be conntected.\n"
            "    fromObject: Scene objects whose output or field serve as input.\n"
            "    fromField: Field or engine output name that will be connected to\n"
            "               toField.\n"
        },
		{"isconnected", (PyCFunction) isconnected, METH_VARARGS,
            "Returns if a field has an incomming connection.\n"
            "\n"
            "Args:\n"
            "    field: Field name whose conntection state will be checked.\n"
            "\n"
            "Returns:\n"
            "    True is the field field has an incoming connection from another\n"
            "    field or engine output.\n"
        },
		{"disconnect", (PyCFunction) disconnect, METH_VARARGS,
            "Disconnects any incoming connection from a field.\n"
            "\n"
            "Args:\n"
            "    field: Field name of field to be disconnected.\n"
        },
		{"set", (PyCFunction) set, METH_VARARGS,
            "Initializes fields or leaf values of a node kit.\n"
            "\n"
            "Args:\n"
            "    Initialization string containing field names and values.\n"
        },
		{"get", (PyCFunction) get, METH_VARARGS,
            "Returns a field or leaf by name.\n"
            "\n"
            "Args:\n"
            "    name: Field or leaf name to be returned.\n"
            "    createIfNeeded: For node kit leafs the second parameter controls\n"
            "                    if the named part should be created if is doesn't\n"
            "                    exist yet.\n"
            "\n"
            "Returns:\n"
            "    Field or node kit leaf is name is given. If no name is passed all\n"
            "    field values are returned as string.\n"
        },
		{"getfields", (PyCFunction) getfields, METH_NOARGS,
            "Return the names and types of this container's fields.\n"
            "\n"
            "Returns:\n"
            "    List of tuples with name and type for each field of this field\n"
            "    container instance.\n"
        },
		{"internal_pointer", (PyCFunction) internal_pointer, METH_NOARGS,
            "Return the internal field container pointer.\n"
            "\n"
            "Returns:\n"
            "    Internal pointer to field container instance.\n"
        },
		{NULL}  /* Sentinel */
	};

	static PyTypeObject fieldContainerType = 
	{
		PyVarObject_HEAD_INIT(NULL, 0)
		"FieldContainer",          /* tp_name */
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
		(getattrofunc) tp_getattro,/* tp_getattro */
		(setattrofunc) tp_setattro,/* tp_setattro */
		0,                         /* tp_as_buffer */
		Py_TPFLAGS_DEFAULT |
		Py_TPFLAGS_BASETYPE,   /* tp_flags */
		"Base class for scene objects of type SoFieldContainer.\n"
        "\n"
        "All field values and node kit parts are dynamically added as class attributes.\n"
        "Please refer to the Open Inventor documentation for the fields of each scene\n"
        "object type.\n"
        ,   /* tp_doc */
		0,                         /* tp_traverse */
		0,                         /* tp_clear */
		0,                         /* tp_richcompare */
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

	return &fieldContainerType;
}


PyTypeObject *PySceneObject::getNodeType()
{
	static PyMethodDef methods[] = 
	{
		{"append", (PyCFunction) append, METH_VARARGS,
            "Appends a scene object to a group node.\n"
            "\n"
            "Args:\n"
            "    Node or sequence of nodes to be added as child(ren).\n"
        },
		{"insert", (PyCFunction) insert, METH_VARARGS,
            "Inserts a scene object into a group node.\n"
            "\n"
            "Args:\n"
            "    index: Position where node will be inserted.\n"
            "    node: Node to be inserted as child.\n"
            "    other: If not None than the node will be inserted relative to this\n"
            "           node, which must be a child of the group.\n"
        },
		{"remove", (PyCFunction) remove, METH_VARARGS,
            "Removes a scene object from a group node.\n"
            "\n"
            "Args:\n"
            "    Index or child node to be removed from group.\n"
        },
		{"node_id", (PyCFunction) node_id, METH_NOARGS,
            "Return the unique node identifier.\n"
            "\n"
            "Returns:\n"
            "    Unique node identifier, which changes with each change of the node\n"
            "    or one of its children.\n"
        },
		{NULL}  /* Sentinel */
	};

	static PySequenceMethods sequence_methods[] = 
	{
		(lenfunc) sq_length,       /* sq_length */
		(binaryfunc) sq_concat,    /* sq_concat */
		0,                         /* sq_repeat */
		(ssizeargfunc) sq_item,    /* sq_item */
		0,                         /* was_sq_slice */
		(ssizeobjargproc) sq_ass_item, /* sq_ass_item */
		0,                         /* was_sq_ass_slice */
		(objobjproc) sq_contains,  /* sq_contains */
		(binaryfunc) sq_inplace_concat, /* sq_inplace_concat */
		0                          /* sq_inplace_repeat */
	};

	static PyTypeObject nodeType = 
	{
		PyVarObject_HEAD_INIT(NULL, 0)
		"Node",                    /* tp_name */
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
		Py_TPFLAGS_BASETYPE,   /* tp_flags */
		"Base class for scene objects of type SoNode.\n"
        "\n"
        "Note that children of group nodes can be accessed as Python sequences,\n"
        "including indexing, slicing, len and del operators.\n", /* tp_doc */
		0,                         /* tp_traverse */
		0,                         /* tp_clear */
		0,                         /* tp_richcompare */
		0,                         /* tp_weaklistoffset */
		0,                         /* tp_iter */
		0,                         /* tp_iternext */
		methods,                   /* tp_methods */
		0,                         /* tp_members */
		0,                         /* tp_getset */
		getFieldContainerType(),   /* tp_base */
		0,                         /* tp_dict */
		0,                         /* tp_descr_get */
		0,                         /* tp_descr_set */
		0,                         /* tp_dictoffset */
		(initproc) tp_init,        /* tp_init */
		0,                         /* tp_alloc */
		tp_new,                    /* tp_new */
	};

	return &nodeType;
}


PyTypeObject *PySceneObject::getEngineType()
{
	static PyTypeObject engineType = 
	{
		PyVarObject_HEAD_INIT(NULL, 0)
		"Engine",                  /* tp_name */
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
		Py_TPFLAGS_BASETYPE,   /* tp_flags */
		"Base class for scene objects of type SoEngine.",  /* tp_doc */
		0,                         /* tp_traverse */
		0,                         /* tp_clear */
		0,                         /* tp_richcompare */
		0,                         /* tp_weaklistoffset */
		0,                         /* tp_iter */
		0,                         /* tp_iternext */
		0,                         /* tp_methods */
		0,                         /* tp_members */
		0,                         /* tp_getset */
		getFieldContainerType(),   /* tp_base */
		0,                         /* tp_dict */
		0,                         /* tp_descr_get */
		0,                         /* tp_descr_set */
		0,                         /* tp_dictoffset */
		(initproc) tp_init,        /* tp_init */
		0,                         /* tp_alloc */
		tp_new,                    /* tp_new */
	};

	return &engineType;
}


PyTypeObject *PySceneObject::getWrapperType(const char *typeName, PyTypeObject *baseType)
{
	static WrapperTypes sceneObjectTypes;

	if (sceneObjectTypes.find(typeName) == sceneObjectTypes.end())
	{
		if (!baseType) return NULL;

		PyTypeObject wrapperType = 
		{
			PyVarObject_HEAD_INIT(NULL, 0)
			typeName,                  /* tp_name */
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
			"Generic Inventor scene object.\n"
            "\n"
            "This Python class wraps a scene object registered in the SoDB. All fields\n"
            "can be accessed via attributes.\n"
            ,   /* tp_doc */
			0,                         /* tp_traverse */
			0,                         /* tp_clear */
			0,                         /* tp_richcompare */
			0,                         /* tp_weaklistoffset */
			0,                         /* tp_iter */
			0,                         /* tp_iternext */
			0,                         /* tp_methods */
			0,                         /* tp_members */
			0,                         /* tp_getset */
			baseType,                  /* tp_base */
			0,                         /* tp_dict */
			0,                         /* tp_descr_get */
			0,                         /* tp_descr_set */
			0,                         /* tp_dictoffset */
			(initproc) tp_init2,       /* tp_init */
			0,                         /* tp_alloc */
			tp_new,                    /* tp_new */
		};
		sceneObjectTypes[typeName] = wrapperType;
	}

	return &sceneObjectTypes[typeName];
}


PyObject *PySceneObject::createWrapper(const char *typeName, SoFieldContainer *instance)
{
	PyObject *obj = 0;
	if (getWrapperType(typeName))
	{
		obj = PyObject_CallObject((PyObject*) getWrapperType(typeName, getNodeType()), NULL);
	}
	else
	{
		PyObject* args = PyTuple_New(1);
		PyTuple_SetItem(args, 0, PyUnicode_FromString(typeName));
		obj = PyObject_CallObject((PyObject*) getNodeType(), args);
		Py_DECREF(args);
	}

	if (obj && instance)
	{
		setInstance((Object*) obj, instance);
	}

	return obj;
}


bool PySceneObject::initNumpy()
{
	if (PyArray_API == NULL)
	{
		import_array1(false);
	}

	return true;
}


void PySceneObject::initSoDB()
{
	if (!SoDB::isInitialized())
	{
		PRESODBINIT();
        
#ifdef TGS_VERSION
		SoDB::threadInit();
		SoInteraction::threadInit();
#else
		SoDB::init();
		SoInteraction::init();
#endif
		// VSG inventor performs HW check in first call to SoGLRenderAction
		SoGLRenderAction aR(SbViewportRegion(1, 1));
	}
}


void PySceneObject::tp_dealloc(Object* self)
{
	if (self->inventorObject)
	{
		self->inventorObject->unref();
	}

	Py_TYPE(self)->tp_free((PyObject*)self);
}


PyObject* PySceneObject::tp_new(PyTypeObject *type, PyObject* /*args*/, PyObject* /*kwds*/)
{
	initSoDB();

	Object *self = (Object *)type->tp_alloc(type, 0);
	if (self != NULL) 
	{
		self->inventorObject = 0;
	}

	return (PyObject *) self;
}


int PySceneObject::tp_init(Object *self, PyObject *args, PyObject *kwds)
{
	char *type = NULL, *name = NULL, *init = NULL;
	PyObject *pointer = NULL;
	static char *kwlist[] = { "type", "init", "name", "pointer", NULL};

	if (! PyArg_ParseTupleAndKeywords(args, kwds, "|sssO", kwlist, &type, &init, &name, &pointer))
		return -1;

	if (self->inventorObject)
	{
		self->inventorObject->unref();
		self->inventorObject = 0;
	}

    
	if (pointer)
	{
		// pointer provided
		if (PyCapsule_CheckExact(pointer))
		{
			void* ptr = PyCapsule_GetPointer(pointer, NULL);
			if (ptr)
			{
				self->inventorObject = (SoFieldContainer *) ptr;
				self->inventorObject->ref();
				if (name && name[0]) self->inventorObject->setName(name);
				if (init && init[0]) setFields(self->inventorObject, init);
			}
		}
	}
	else if (type && type[0])
	{
		// create new instance
		SoType t = SoType::fromName(type);
		if (t.canCreateInstance())
		{
			self->inventorObject = (SoFieldContainer *) t.createInstance();
			if (self->inventorObject)
			{
				self->inventorObject->ref();
				if ((PyEngine_Check(self) && self->inventorObject->isOfType(SoEngine::getClassTypeId())) || 
					(PyNode_Check(self) && self->inventorObject->isOfType(SoNode::getClassTypeId())) )
				{
					if (name && name[0]) self->inventorObject->setName(name);
					if (init && init[0]) self->inventorObject->set(init);
				}
				else
				{
					self->inventorObject->unref();
					self->inventorObject = 0;
					PyErr_SetString(PyExc_TypeError, "Incorrect scene object type (must be node or engine)");
				}
			}
		}
	}
	else if (name)
	{
		// instance lookup by name
		if (PyNode_Check(self))
		{
			self->inventorObject = SoNode::getByName(name);
		}
		else if (PyEngine_Check(self))
		{
			self->inventorObject = SoEngine::getByName(name);
		}

		if (self->inventorObject)
		{
			self->inventorObject->ref();
		}
		else
		{
			PyErr_SetString(PyExc_ValueError, "No node or engine with given name exists");
		}
	}

	initDictionary(self);

	return 0;
}


void PySceneObject::initDictionary(Object *self)
{
	if (self->inventorObject)
	{
		SoFieldList lst;
		self->inventorObject->getFields(lst);
		for (int i = 0; i < lst.getLength(); ++i)
		{
			SbName name;
			self->inventorObject->getFieldName(lst[i], name);
			SbString descr("Field of type ");
			descr += lst[i]->getTypeId().getName().getString();
			PyDict_SetItem(Py_TYPE(self)->tp_dict, PyUnicode_FromString(name.getString()), PyUnicode_FromString(descr.getString()));
		}
	}
}


int PySceneObject::setFields(SoFieldContainer *fieldContainer, char *value)
{
	if (!fieldContainer) 
		return FALSE;

	if (fieldContainer->isOfType(SoBaseKit::getClassTypeId()))
	{
		SoBaseKit *kit = (SoBaseKit *) fieldContainer;
		return kit->set(value);		
	}

	return fieldContainer->set(value);
}


int PySceneObject::tp_init2(Object *self, PyObject *args, PyObject *kwds)
{
	char *type = NULL, *name = NULL, *init = NULL;
	PyObject *pointer = NULL;
	static char *kwlist[] = { "init", "name", "pointer", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|ssO", kwlist, &init, &name, &pointer))
		return -1;

	if (self->inventorObject)
	{
		self->inventorObject->unref();
		self->inventorObject = 0;
	}

	if (pointer)
	{
		// pointer provided
		if (PyCapsule_CheckExact(pointer))
		{
			void* ptr = PyCapsule_GetPointer(pointer, NULL);
			if (ptr)
			{
				self->inventorObject = (SoFieldContainer *) ptr;
				self->inventorObject->ref();
				if (name && name[0]) self->inventorObject->setName(name);
				if (init && init[0]) setFields(self->inventorObject, init);
			}
		}
	}
	else
	{
		PyObject *classObj = PyObject_GetAttrString((PyObject*) self, "__class__");
		if (classObj != NULL)
		{
			PyObject *className = PyObject_GetAttrString(classObj, "__name__");
			if (className != NULL)
			{
				// create new instance
				type = PyUnicode_AsUTF8(className);
				SoType t = SoType::fromName(type);
				if (t.canCreateInstance())
				{
					self->inventorObject = (SoFieldContainer *) t.createInstance();
					if (self->inventorObject)
					{
						self->inventorObject->ref();
						if ((PyEngine_Check(self) && self->inventorObject->isOfType(SoEngine::getClassTypeId())) || 
							(PyNode_Check(self) && self->inventorObject->isOfType(SoNode::getClassTypeId())) )
						{
							if (name && name[0]) self->inventorObject->setName(name);
							if (init && init[0]) setFields(self->inventorObject, init);
						}
						else
						{
							self->inventorObject->unref();
							self->inventorObject = 0;
							PyErr_SetString(PyExc_TypeError, "Incorrect scene object type (must be node or engine)");
						}
					}
				}
			}

			Py_DECREF(className);
		}
		Py_DECREF(classObj);
	}

	initDictionary(self);

	return 0;
}


void PySceneObject::setInstance(Object *self, SoFieldContainer *obj)
{
	if (self->inventorObject)
	{
		self->inventorObject->unref();
		self->inventorObject = 0;
	}

	if (obj)
	{
		self->inventorObject = obj;
		self->inventorObject->ref();
	}
}


PyObject *PySceneObject::getField(SoField *field)
{
	initNumpy();
	PyObject *result = NULL;

	if (field->isOfType(SoSFNode::getClassTypeId()))
	{
        SoSFNode *nodeField = (SoSFNode*) field;
        SoNode *node = nodeField->getValue();
        if (node)
        {
            PyObject *found = createWrapper(node->getTypeId().getName().getString(), node);
            if (found)
            {
                return found;
            }
        }
        Py_INCREF(Py_None);
        return Py_None;
	}
	else if (field->isOfType(SoSFMatrix::getClassTypeId()))
	{
        // special case for single matrix: return as [4][4] rather than [16]
		npy_intp dims[] = { 4, 4 };
		PyArrayObject *arr = (PyArrayObject*) PyArray_SimpleNewFromData(2, dims, NPY_FLOAT32, (void*) ((SoSFMatrix *) field)->getValue().getValue());
		return PyArray_Return(arr);
	}
	else if (field->isOfType(SoSFImage::getClassTypeId()))
	{
		SbVec2s size;
		int nc;
		const unsigned char *pixel = ((SoSFImage*) field)->getValue(size, nc);
		if (pixel)
		{
			PyObject *obj = PyTuple_New(4);
			PyTuple_SetItem(obj, 0, PyLong_FromLong(size[0]));
			PyTuple_SetItem(obj, 1, PyLong_FromLong(size[1]));
			PyTuple_SetItem(obj, 2, PyLong_FromLong(nc));

			npy_intp dims[] = { size[0] * size[1] * nc };
			PyTuple_SetItem(obj, 3, PyArray_SimpleNewFromData(1, dims, NPY_UBYTE, (void*) pixel));

			return obj;
		}

		Py_INCREF(Py_None);
		return Py_None;
	}
	else if (field->isOfType(SoSFPlane::getClassTypeId()))
	{
		npy_intp dims[] = { 4 };
		SbPlane p = ((SoSFPlane*) field)->getValue();
		SbVec4f v;
		v[0] = p.getNormal()[0]; v[1] = p.getNormal()[1]; v[2] = p.getNormal()[2];
		v[3] = p.getDistanceFromOrigin();
		PyArrayObject *arr = (PyArrayObject*) PyArray_SimpleNewFromData(1, dims, NPY_FLOAT32, (void*) v.getValue());
		return PyArray_Return(arr);
	}
	else if (field->isOfType(SoMFPlane::getClassTypeId()))
	{
		npy_intp dims[] = { ((SoMFPlane*) field)->getNum() , 4 };
		PyArrayObject *arr = (PyArrayObject*) PyArray_SimpleNew(2, dims, NPY_FLOAT32);
		float *data = (float *) PyArray_GETPTR1(arr, 0);
		for (int i = 0; data && (i < dims[0]); ++i)
		{
			SbPlane p = *((SoMFPlane *) field)->getValues(i);
			data[i * 4 + 0] = p.getNormal()[0];
			data[i * 4 + 1] = p.getNormal()[1]; 
			data[i * 4 + 2] = p.getNormal()[2];
			data[i * 4 + 3] = p.getDistanceFromOrigin();
		}
		return PyArray_Return(arr);			
	}
	else if (field->isOfType(SoSFString::getClassTypeId()))
	{
		SbString s = ((SoSFString*) field)->getValue();
		#ifdef TGS_VERSION
		result = PyUnicode_FromUnicode(s.toWideChar(), s.getLength());
		#else
		result = PyUnicode_FromStringAndSize(s.getString(), s.getLength());
		#endif
		return result;
	}
	else if (field->isOfType(SoMFString::getClassTypeId()))
	{
		result = PyList_New(((SoMField*) field)->getNum());
		for (int i = 0; i < ((SoMField*) field)->getNum(); ++i)
		{
			SbString s = *(((SoMFString*) field)->getValues(i));
			PyList_SetItem(result, i,
				#ifdef TGS_VERSION
				PyUnicode_FromUnicode(s.toWideChar(), s.getLength())
				#else
				PyUnicode_FromStringAndSize(s.getString(), s.getLength())
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
		result = PyList_New(((SoMField*) field)->getNum());
		for (int i = 0; i < ((SoMField*) field)->getNum(); ++i)
		{
			SbString s;
			((SoMField*) field)->get1(i, s);
			PyList_SetItem(result, i, PyUnicode_FromStringAndSize(s.getString(), s.getLength()));
		}
		return result;
	}

	// generic string based fallback
	SbString s;
	field->get(s);
	result = PyUnicode_FromStringAndSize(s.getString(), s.getLength());

	return result;
}


int PySceneObject::setField(SoField *field, PyObject *value)
{
	initNumpy();
	int result = 0;

	if (field->isOfType(SoSFNode::getClassTypeId()))
	{
        SoSFNode *nodeField = (SoSFNode*) field;
        if (PyNode_Check(value))
		{
			Object *child = (Object *) value;
			if (child->inventorObject && child->inventorObject->isOfType(SoNode::getClassTypeId()))
			{
				if (field->getContainer() && field->getContainer()->isOfType(SoBaseKit::getClassTypeId()))
				{
					SoBaseKit *baseKit = (SoBaseKit *) field->getContainer();
					SbName fieldName;
					if (baseKit->getFieldName(field, fieldName))
					{
						if (baseKit->getNodekitCatalog()->isLeaf(fieldName))
						{
							// update part of a node kit
							baseKit->setPart(fieldName, (SoNode*) child->inventorObject);
							return result;
						}
					}
				}

                nodeField->setValue((SoNode*) child->inventorObject);
			}
		}
        else
        {
            nodeField->setValue(0);
        }
	}
	else if (field->isOfType(SoSFString::getClassTypeId()))
	{
		PyObject *str = PyObject_Str(value);
		if (str)
		{
			Py_ssize_t len;
			#ifdef TGS_VERSION
			((SoSFString*) field)->setValue(PyUnicode_AsWideCharString(str, &len));
			#else
			((SoSFString*) field)->setValue(PyUnicode_AsUTF8AndSize(str, &len));
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
			((SoMFString*) field)->setNum(n);

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
						((SoMFString*) field)->set1Value(i, PyUnicode_AsWideCharString(str, &len));
						#else
						((SoMFString*) field)->set1Value(i, PyUnicode_AsUTF8AndSize(str, &len));
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
				((SoMFString*) field)->setValue(PyUnicode_AsWideCharString(str, &len));
				#else
				((SoMFString*) field)->setValue(PyUnicode_AsUTF8AndSize(str, &len));
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
				if ((width * height * nc) > 0 )
				{
					PyArrayObject *arr = (PyArrayObject*) PyArray_FROM_OTF(pixelObj, NPY_UBYTE, NPY_ARRAY_IN_ARRAY | NPY_ARRAY_FORCECAST);
					if (arr)
					{
						size_t n = PyArray_SIZE(arr);
						if ((width * height * nc) <= n)
						{
							const unsigned char *data = (const unsigned char *) PyArray_GETPTR1(arr, 0);
							((SoSFImage*) field)->setValue(SbVec2s(width, height), nc, data);
						}

						Py_DECREF(arr);
					}
				}
				else
				{
					((SoSFImage*) field)->setValue(SbVec2s(0, 0), 0, 0);
				}
			}
		}
		else if (field->isOfType(SoSFPlane::getClassTypeId()) || field->isOfType(SoMFPlane::getClassTypeId()))
		{
			PyArrayObject *arr = (PyArrayObject*) PyArray_FROM_OTF(value, NPY_FLOAT32, NPY_ARRAY_IN_ARRAY | NPY_ARRAY_FORCECAST);
			if (arr)
			{
				size_t n = PyArray_SIZE(arr);
				float *data = (float *) PyArray_GETPTR1(arr, 0);

				if (field->isOfType(SoSFPlane::getClassTypeId()))
				{
					if (n == 4)
					{
						((SoSFPlane*) field)->setValue(SbPlane(SbVec3f(data[0], data[1], data[2]), data[3]));
					}
				}
				else if (field->isOfType(SoMFPlane::getClassTypeId()))
				{
					if ((n % 4) == 0)
					{
						((SoMFPlane*) field)->setNum(n / 4);
						for (int i = 0; i < (n / 4); ++i)
						{
							float *p = data + (i * 3);
							((SoMFPlane*) field)->set1Value(i, SbPlane(SbVec3f(p[0], p[1], p[2]), p[3]));
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
					((SoSFRotation*) field)->setValue(SbVec3f(axisValue), angle);
					valueWasSet = true;
				}
			}
			else if (PyArg_ParseTuple(value, "OO", &fromVec, &toVec))
			{
				// from & to vectors
				float fromValue[3], toValue[3];
				if (getFloatsFromPyObject(fromVec, 3, fromValue) && getFloatsFromPyObject(toVec, 3, toValue))
				{
					((SoSFRotation*) field)->setValue(SbRotation(SbVec3f(fromValue), SbVec3f(toValue)));
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
					((SoSFRotation*) field)->setValue(SbRotation(m));
				}
				else if (getFloatsFromPyObject(axis, 4, value))
				{
					// quaternion
					((SoSFRotation*) field)->setValue(value);
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
			((SoMFString*) field)->setNum(n);

			for (size_t i = 0; i < n; ++i)
			{
				PyObject *item = PySequence_GetItem(seq, i);
				if (item)
				{
					PyObject *str = PyObject_Str(item);
					if (str)
					{
						Py_ssize_t len;
						((SoMField*) field)->set1(i, PyUnicode_AsUTF8AndSize(str, &len));
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


PyObject* PySceneObject::tp_getattro(Object* self, PyObject *attrname)
{
	const char *fieldName = PyUnicode_AsUTF8(attrname);
	if (self->inventorObject && fieldName)
	{
		if (self->inventorObject->isOfType(SoBaseKit::getClassTypeId()))
		{
			SoBaseKit *baseKit = (SoBaseKit *) self->inventorObject;
			if (baseKit->getNodekitCatalog()->isLeaf(fieldName))
			{
				SoNode *node = baseKit->getPart(fieldName, TRUE);
				if (node)
				{
					PyObject *obj = createWrapper(node->getTypeId().getName().getString(), node);
					if (obj)
					{
						return obj;
					}
				}
			}
		}
		else
		{
			SoField *field = self->inventorObject->getField(fieldName);
			if (field)
			{
				return getField(field);
			}
		}
	}

	return PyObject_GenericGetAttr((PyObject*) self, attrname);
}


int PySceneObject::tp_setattro(Object* self, PyObject *attrname, PyObject *value)
{
	const char *fieldName = PyUnicode_AsUTF8(attrname);
	if (self->inventorObject && fieldName)
	{
		SoField *field = self->inventorObject->getField(fieldName);
		if (field)
		{
			return setField(field, value);
		}
	}

	return PyObject_GenericSetAttr((PyObject*) self, attrname, value);
}


Py_ssize_t PySceneObject::sq_length(Object *self)
{
	if (self->inventorObject && self->inventorObject->isOfType(SoGroup::getClassTypeId()))
	{
		return ((SoGroup*) self->inventorObject)->getNumChildren();
	}

	return 0;
}


PyObject * PySceneObject::sq_concat(Object *self, PyObject *item)
{
	if (self->inventorObject && self->inventorObject->isOfType(SoGroup::getClassTypeId()))
	{
		PyObject *obj = createWrapper(self->inventorObject->getTypeId().getName().getString());
		if (obj)
		{
			sq_inplace_concat((Object*) obj, (PyObject*) self);
			sq_inplace_concat((Object*) obj, item);

			return obj;
		}
	}

	Py_INCREF(Py_None);
	return Py_None;
}


PyObject * PySceneObject::sq_inplace_concat(Object *self, PyObject *item)
{
	if (item && PyNode_Check(item))
	{
		Object *child = (Object *) item;
		if (self->inventorObject && self->inventorObject->isOfType(SoGroup::getClassTypeId()) && 
			child->inventorObject && child->inventorObject->isOfType(SoNode::getClassTypeId()))
		{
			((SoGroup*) self->inventorObject)->addChild((SoNode*) child->inventorObject);
		}
	}
	else if (item && PySequence_Check(item))
	{
		PyObject *seq = PySequence_Fast(item, "expected a sequence");
		size_t n = PySequence_Size(seq);

		for (size_t i = 0; i < n; ++i)
		{
			PyObject *seqItem = PySequence_GetItem(seq, i);
			if (seqItem && PyNode_Check(seqItem))
			{
				Object *child = (Object *) seqItem;
				if (self->inventorObject && self->inventorObject->isOfType(SoGroup::getClassTypeId()) && 
					child->inventorObject && child->inventorObject->isOfType(SoNode::getClassTypeId()))
				{
					((SoGroup*) self->inventorObject)->addChild((SoNode*) child->inventorObject);
				}
			}
		}

		Py_XDECREF(seq);
	}

	Py_INCREF(self);
	return ((PyObject*) self);
}


int PySceneObject::sq_contains(Object *self, PyObject *item)
{
	if (item && PyNode_Check(item))
	{
		Object *child = (Object *) item;
		if (self->inventorObject && self->inventorObject->isOfType(SoGroup::getClassTypeId()) && 
			child->inventorObject && child->inventorObject->isOfType(SoNode::getClassTypeId()))
		{
			return (((SoGroup*) self->inventorObject)->findChild((SoNode*) child->inventorObject) >= 0);
		}
	}

	return 0;
}


PyObject *PySceneObject::sq_item(Object *self, Py_ssize_t idx)
{
	if (self->inventorObject && self->inventorObject->isOfType(SoGroup::getClassTypeId()))
	{
		if (idx < ((SoGroup*) self->inventorObject)->getNumChildren())
		{
			SoNode *node = ((SoGroup*) self->inventorObject)->getChild(idx);
			PyObject *obj = createWrapper(node->getTypeId().getName().getString(), node);
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


int PySceneObject::sq_ass_item(Object *self, Py_ssize_t idx, PyObject *item)
{
	if (self->inventorObject && self->inventorObject->isOfType(SoGroup::getClassTypeId()))
	{
		if (item == NULL)
		{
			// remove
			((SoGroup*) self->inventorObject)->removeChild(idx);
			return 0;
		}
		else if (PySceneObject_Check(item))
		{
			if (((Object*) item)->inventorObject && ((Object*) item)->inventorObject->isOfType(SoNode::getClassTypeId()))
			{
				if (idx < ((SoGroup*) self->inventorObject)->getNumChildren())
				{
					// replace
					((SoGroup*) self->inventorObject)->replaceChild(idx, (SoNode*) ((Object*) item)->inventorObject);
				}
				else
				{
					// add
					((SoGroup*) self->inventorObject)->addChild((SoNode*) ((Object*) item)->inventorObject);
				}
				return 0;
			}
		}
	}
	else
	{
		PyErr_SetString(PyExc_TypeError, "Not of type SoGroup");
	}

	return -1;
}


PyObject* PySceneObject::append(Object *self, PyObject *args)
{
	PyObject *item = 0;
	if (PyArg_ParseTuple(args, "O", &item))
	{
		sq_inplace_concat(self, item);
	}

	Py_INCREF(Py_None);
	return Py_None;
}


PyObject* PySceneObject::insert(Object *self, PyObject *args)
{
	PyObject *item = 0, *base = 0;
	int idx = 0;

	if (self->inventorObject && PyArg_ParseTuple(args, "iO|O", &idx, &item, &base))
	{
		SoGroup *group = 0;
		if (base)
		{
			// find base and insert relative to that
			SoSearchAction sa;
			if (PyNode_Check(base))
			{
				sa.setNode((SoNode*) ((Object *) base)->inventorObject);
			}
			else if (PySequence_Check(base))
			{
				PyObject *seqItem = PySequence_GetItem(base, 0);
				if (PyNode_Check(seqItem))
				{
					sa.setNode((SoNode*) ((Object *) seqItem)->inventorObject);
				}
			}
			sa.setInterest(SoSearchAction::FIRST);
			sa.apply((SoNode*) self->inventorObject);
			if (sa.getPath())
			{
				group = (SoGroup*) sa.getPath()->getNodeFromTail(1);
				idx += group->findChild(sa.getNode());
			}
		}
		else
		{
			// insert to self
			group = (SoGroup*) self->inventorObject;
			if (idx < 0)
			{
				idx = group->getNumChildren() + idx;
			}
		}

		if (group && group->isOfType(SoGroup::getClassTypeId()))
		{
			if (item && PyNode_Check(item))
			{
				Object *child = (Object *) item;
				group->insertChild((SoNode*) child->inventorObject, idx);
			}
			else if (item && PySequence_Check(item))
			{
				PyObject *seq = PySequence_Fast(item, "expected a sequence");
				size_t n = PySequence_Size(seq);

				for (size_t i = 0; i < n; ++i)
				{
					PyObject *seqItem = PySequence_GetItem(seq, i);
					if (seqItem && PyNode_Check(seqItem))
					{
						Object *child = (Object *) seqItem;
						group->insertChild((SoNode*) child->inventorObject, idx++);
					}
				}

				Py_XDECREF(seq);
			}
		}
	}

	Py_INCREF(Py_None);
	return Py_None;
}


PyObject* PySceneObject::remove(Object *self, PyObject *args)
{
	if (self->inventorObject && self->inventorObject->isOfType(SoGroup::getClassTypeId()))
	{
		PyObject *item = 0;
		if (PyArg_ParseTuple(args, "O", &item))
		{
			if (PyLong_Check(item))
			{
				SoGroup *group = (SoGroup*) self->inventorObject;
				int idx = (int) PyLong_AsLong(item);
				group->removeChild(idx < 0 ? group->getNumChildren() + idx : idx);
			}
			else if (item && PyNode_Check(item))
			{
				Object *child = (Object *) item;
				if (child->inventorObject && child->inventorObject->isOfType(SoNode::getClassTypeId()))
				{
					((SoGroup*) self->inventorObject)->removeChild((SoNode*) child->inventorObject);
				}
			}
		}
		else
		{
			((SoGroup*) self->inventorObject)->removeAllChildren();
		}
	}

	Py_INCREF(Py_None);
	return Py_None;
}


PyObject* PySceneObject::setname(Object* self, PyObject *args)
{
	char *name = 0;
	if (self->inventorObject && PyArg_ParseTuple(args, "s", &name))
	{
		self->inventorObject->setName(SbName(name));
	}

	Py_INCREF(Py_None);
	return Py_None;
}


PyObject* PySceneObject::getname(Object* self)
{
	SbName n("");

	if (self->inventorObject)
	{
		n = self->inventorObject->getName();
	}

	return _PyUnicode_FromASCII(n.getString(), n.getLength());
}


PyObject* PySceneObject::sotype(Object* self)
{
	SbName n("");

	if (self->inventorObject)
	{
		n = self->inventorObject->getTypeId().getName();
	}

	return _PyUnicode_FromASCII(n.getString(), n.getLength());
}


PyObject* PySceneObject::node_id(Object* self)
{
	long id = 0;

	if (self->inventorObject && self->inventorObject->isOfType(SoNode::getClassTypeId()))
	{
		id = ((SoNode*) self->inventorObject)->getNodeId();
	}

	return PyLong_FromLong(id);
}


PyObject* PySceneObject::touch(Object* self)
{
	if (self->inventorObject)
	{
		self->inventorObject->touch();
	}

	Py_INCREF(Py_None);
	return Py_None;
}


PyObject* PySceneObject::enable_notify(Object* self, PyObject *args)
{
	long enabled = 0;
	bool enable = 0;
	if (self->inventorObject)
	{
		if (PyArg_ParseTuple(args, "p", &enable))
		{
			enabled = self->inventorObject->enableNotify(enable);
		}
		else
		{
			enabled = self->inventorObject->isNotifyEnabled();
		}
	}

	return PyBool_FromLong(enabled);
}


PyObject* PySceneObject::connect(Object *self, PyObject *args)
{
	long connected = 0;
	PyObject *sceneObject = NULL;
	char *fieldToName = 0, *fieldFromName = 0;
	if (self->inventorObject && PyArg_ParseTuple(args, "sOs", &fieldToName, &sceneObject, &fieldFromName))
	{
		if (sceneObject && PySceneObject_Check(sceneObject) && fieldToName && fieldFromName)
		{
			if (((Object*) sceneObject)->inventorObject)
			{
				SoField *fieldTo = 0, *fieldFrom = 0;

				fieldTo = self->inventorObject->getField(fieldToName);
				fieldFrom = ((Object*) sceneObject)->inventorObject->getField(fieldFromName);

				if (fieldTo && fieldFrom)
				{
					fieldTo->connectFrom(fieldFrom);
					connected = 1;
				}
				else if (!fieldFrom && ((Object*) sceneObject)->inventorObject->isOfType(SoEngine::getClassTypeId()))
				{
					SoEngineOutput *outputFrom = ((SoEngine*) ((Object*) sceneObject)->inventorObject)->getOutput(fieldFromName);
					if (fieldTo && outputFrom)
					{
						fieldTo->connectFrom(outputFrom);
						connected = 1;
					}
				}
			}
		}
	}

	return PyBool_FromLong(connected);
}


PyObject* PySceneObject::disconnect(Object *self, PyObject *args)
{
	char *fieldToName = 0;
	if (self->inventorObject && PyArg_ParseTuple(args, "s", &fieldToName))
	{
		if (fieldToName)
		{
			SoField *fieldTo = self->inventorObject->getField(fieldToName);
			if (fieldTo)
			{
				fieldTo->disconnect();
			}
		}
	}

	Py_INCREF(Py_None);
	return Py_None;
}


PyObject* PySceneObject::isconnected(Object *self, PyObject *args)
{
	long connected = 0;

	char *fieldToName = 0;
	if (self->inventorObject && PyArg_ParseTuple(args, "s", &fieldToName))
	{
		if (fieldToName)
		{
			SoField *fieldTo = self->inventorObject->getField(fieldToName);
			if (fieldTo)
			{
				connected = fieldTo->isConnected();
			}
		}
	}

	return PyBool_FromLong(connected);
}


PyObject* PySceneObject::set(Object *self, PyObject *args)
{
	char *value = 0;
	if (self->inventorObject && PyArg_ParseTuple(args, "s", &value))
	{
		setFields(self->inventorObject, value);
	}

	Py_INCREF(Py_None);
	return Py_None;
}


PyObject* PySceneObject::get(Object *self, PyObject *args)
{
	char *name = 0;
	bool createIfNeeded = true;
	if (self->inventorObject)
	{
		if (PyArg_ParseTuple(args, "|sp", &name, &createIfNeeded))
		{
            if (name)
            {
                // name given
                if (self->inventorObject->isOfType(SoBaseKit::getClassTypeId()))
                {
                    // return leaf?
                    SoBaseKit *baseKit = (SoBaseKit *) self->inventorObject;
                    if (baseKit->getNodekitCatalog()->isLeaf(name))
                    {
                        SoNode *node = baseKit->getPart(name, createIfNeeded ? TRUE : FALSE);
                        if (node)
                        {
                            PyObject *obj = createWrapper(node->getTypeId().getName().getString(), node);
                            if (obj)
                            {
                                return obj;
                            }
                        }
                    }
                }

                // return field?
                SoField *field = self->inventorObject->getField(name);
                if (field)
                {
                    return getField(field);
                }
            }
            else
            {
                // no name then return all field values
                if (self->inventorObject)
                {
                    SbString value;
                    self->inventorObject->get(value);
            
                    return _PyUnicode_FromASCII(value.getString(), value.getLength());
                }
            }
        }
	}

	Py_INCREF(Py_None);
	return Py_None;
}


PyObject* PySceneObject::getfields(Object* self)
{
	if (self->inventorObject)
	{
        SoFieldList fieldList;
        int numFields = self->inventorObject->getFields(fieldList);
        
        PyObject *result = PyList_New(numFields);
		for (int i = 0; i < numFields; ++i)
		{
            PyObject* fieldNameType = PyTuple_New(2);
            SbName fieldName("");
            if (self->inventorObject->getFieldName(fieldList[i], fieldName))
            {
                PyTuple_SetItem(fieldNameType, 0, PyUnicode_FromString(fieldName.getString()));
            }
            else
            {
                PyTuple_SetItem(fieldNameType, 0, PyUnicode_FromString(""));
            }
            PyTuple_SetItem(fieldNameType, 1, PyUnicode_FromString(fieldList[i]->getTypeId().getName().getString()));
            PyList_SetItem(result, i, fieldNameType);
		}
		return result;
	}
    
	Py_INCREF(Py_None);
	return Py_None;
}


PyObject* PySceneObject::internal_pointer(Object* self)
{
	if (self->inventorObject)
	{
        return PyCapsule_New((void*) self->inventorObject, NULL, NULL);
	}

	Py_INCREF(Py_None);
	return Py_None;
}


bool PySceneObject::getFloatsFromPyObject(PyObject *obj, int size, float *value_out)
{
	initNumpy();

	if (obj)
	{
		if (PyArrayObject *arr = (PyArrayObject*) PyArray_FROM_OTF(obj, NPY_FLOAT32, NPY_ARRAY_IN_ARRAY | NPY_ARRAY_FORCECAST))
		{
			if (PyArray_SIZE(arr) == size)
			{
				memcpy(value_out, PyArray_BYTES(arr), sizeof(float) * size);
			}
			Py_DECREF(arr);

			return true;
		}
	}

	return false;
}


PyObject *PySceneObject::getPyObjectFromFloats(const float *value_in, int size)
{
	initNumpy();
    
    npy_intp dims[] = { size };
    PyObject *arr = (PyObject*) PyArray_SimpleNewFromData(1, dims, NPY_FLOAT32, (void*) value_in);
    
    return arr;
}

