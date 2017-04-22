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
#include <Inventor/engines/SoEngine.h>
#include <Inventor/fields/SoFields.h>
#include <Inventor/actions/SoSearchAction.h>
#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/SoDB.h>
#include <Inventor/SoInteraction.h>
#include <Inventor/errors/SoErrors.h>


#ifdef __COIN__
#include <Inventor/annex/ForeignFiles/SoForeignFileKit.h>
#endif


#include <map>
#include <string>

#include "PySceneObject.h"
#include "PyField.h"
#include "PyEngineOutput.h"

#pragma warning ( disable : 4127 ) // conditional expression is constant in Py_DECREF


typedef std::map<std::string, PyTypeObject> WrapperTypes;


// by defining PRESODBINIT an external initialization function can configured
#ifdef PRESODBINIT
#ifdef _WIN32
void __declspec( dllimport ) PRESODBINIT();
#endif
#else
#define PRESODBINIT() /* unused */
#endif



// reports all Inventor errors with PyErr_SetString()
static void inventorErrorCallback(const SoError *error, void *data)
{
	SbString str = error->getDebugString();

	int trim = str.getLength();
	while ((trim > 0) && strchr(" \t\n\r", str[trim - 1]))
		trim--;

	if (trim < str.getLength())
		str.deleteSubString(trim);

	if (error->isOfType(SoDebugError::getClassTypeId()))
	{
		switch (((SoDebugError*) error)->getSeverity())
		{
		case SoDebugError::INFO:	return; // not an error: don't report
		case SoDebugError::WARNING:	PyErr_SetString(PyExc_Warning, str.getString()); return;
		}

		PyErr_SetString(PyExc_AssertionError, str.getString());
	}
	else if (error->isOfType(SoMemoryError::getClassTypeId()))
	{
		PyErr_SetString(PyExc_MemoryError, str.getString());
	}
	else if (error->isOfType(SoReadError::getClassTypeId()))
	{
		PyErr_SetString(PyExc_SyntaxError, str.getString());
	}

	PyErr_SetString(PyExc_Exception, str.getString());
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
		{"check_type", (PyCFunction) check_type, METH_VARARGS,
            "Checks if a scene object is derived form a given type.\n"
            "\n"
            "Args:\n"
            "    Name of type to check for as string.\n"
			"\n"
            "Returns:\n"
            "    True if the instance is derived from the given type.\n"
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
            "Initializes fields or parts of a node kit.\n"
            "\n"
            "Args:\n"
            "    Initialization string containing field names and values.\n"
        },
		{"get", (PyCFunction) get, METH_VARARGS,
            "Returns a field or part by name.\n"
            "\n"
            "Args:\n"
            "    name: Field or part name to be returned.\n"
            "    createIfNeeded: For node kit parts the second parameter controls\n"
            "                    if the named part should be created if is doesn't\n"
            "                    exist yet.\n"
            "\n"
            "Returns:\n"
            "    Field or node kit part if name is given. If no name is passed all\n"
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
		(reprfunc) tp_repr,        /* tp_repr */
		0,                         /* tp_as_number */
		0,                         /* tp_as_sequence */
		0,                         /* tp_as_mapping */
		0,                         /* tp_hash  */
		0,                         /* tp_call */
		(reprfunc) tp_str,         /* tp_str */
		(getattrofunc) tp_getattro,/* tp_getattro */
		(setattrofunc) tp_setattro,/* tp_setattro */
		0,                         /* tp_as_buffer */
		Py_TPFLAGS_DEFAULT |
		Py_TPFLAGS_BASETYPE,   /* tp_flags */
		"Base class for scene objects of type SoFieldContainer.\n"
        "\n"
        "All field values and node kit parts are dynamically added as class\n"
        "attributes. Please refer to the Open Inventor documentation for the\n"
        "fields of each scene object type.\n"
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

	static PyMappingMethods mapping_methods[] = 
	{
		(lenfunc) mp_length,
		(binaryfunc) mp_subscript,
		(objobjargproc)	mp_ass_subscript
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
		(reprfunc) tp_repr,        /* tp_repr */
		0,                         /* tp_as_number */
		sequence_methods,          /* tp_as_sequence */
		mapping_methods,           /* tp_as_mapping */
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
    static PyMethodDef methods[] =
    {
        { "getoutputs", (PyCFunction)getoutputs, METH_NOARGS,
        "Return the names of this engines' outputs.\n"
        "\n"
        "Returns:\n"
        "    List of tuples with name and type for each engine output.\n"
        },
        { NULL }  /* Sentinel */
    };

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
		(reprfunc) tp_repr,        /* tp_repr */
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
			(reprfunc) tp_repr,        /* tp_repr */
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

		// register callbacks to report them Python interface
		SoError::setHandlerCallback(inventorErrorCallback, 0);
		SoDebugError::setHandlerCallback(inventorErrorCallback, 0);
		SoMemoryError::setHandlerCallback(inventorErrorCallback, 0);
		SoReadError::setHandlerCallback(inventorErrorCallback, 0);
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
		else
		{
			PyErr_SetString(PyExc_TypeError, "Name lookup requires Node or Engine instance");
			return -1;
		}

		if (self->inventorObject)
		{
			self->inventorObject->ref();
		}
		else
		{
			PyErr_SetString(PyExc_ValueError, "No scene object with given name exists");
			return -1;
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


PyObject* PySceneObject::tp_repr(Object *self)
{
	if (self->inventorObject)
	{
		SbString type = self->inventorObject->getTypeId().getName().getString();
		SbString name = self->inventorObject->getName().getString();
		SbString repr;

		if (name.getLength())
		{
			repr.sprintf("<%s \"%s\" at %p>", type.getString(), name.getString(), self->inventorObject);
		}
		else
		{
			repr.sprintf("<%s at %p>", type.getString(), self->inventorObject);
		}

		return PyUnicode_FromString(repr.getString());
	}

	return PyUnicode_FromString("Uninitialized");
}


PyObject* PySceneObject::tp_str(Object *self)
{
    if (self->inventorObject)
    {
		SbString type = self->inventorObject->getTypeId().getName().getString();
		SbString name = self->inventorObject->getName().getString();
		SbString repr;

        SbString value;
        self->inventorObject->get(value);
		SbBool hasValues = value.getLength() > 1;

		if (name.getLength())
		{
			repr.sprintf("<%s \"%s\" at %p>%s%s", type.getString(), name.getString(), self->inventorObject,
				hasValues ? "\n" : "", hasValues ? value.getString() : "");
		}
		else
		{
			repr.sprintf("<%s at %p>%s%s", type.getString(), self->inventorObject,
				hasValues ? "\n" : "", hasValues ? value.getString() : "");
		}

        return PyUnicode_FromString(repr.getString());
    }

	return tp_repr(self);
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


PyObject* PySceneObject::tp_getattro(Object* self, PyObject *attrname)
{
	const char *fieldName = PyUnicode_AsUTF8(attrname);
	if (self->inventorObject && fieldName)
	{
		#ifdef __COIN__
		// special case for SoForeignFileKit: "convert" returns converted scene graph
		if (self->inventorObject->isOfType(SoForeignFileKit::getClassTypeId()))
		{
			if  (SbString("convert") == fieldName)
			{
				SoNode *node = ((SoForeignFileKit*) self->inventorObject)->convert();
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
		#endif

		SoField *field = self->inventorObject->getField(fieldName);
		if (field)
		{
			if (self->inventorObject->isOfType(SoBaseKit::getClassTypeId()))
			{
				SoBaseKit *baseKit = (SoBaseKit *) self->inventorObject;
				if (baseKit->getNodekitCatalog()->getPartNumber(fieldName) >= 0)
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

					// part is NULL
					Py_INCREF(Py_None);
					return Py_None;
				}
			}

			return PyField::getFieldValue(field);
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
			return PyField::setFieldValue(field, value);
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


Py_ssize_t PySceneObject::mp_length(Object *self)
{
	if (self->inventorObject && self->inventorObject->isOfType(SoGroup::getClassTypeId()))
	{
		return ((SoGroup*) self->inventorObject)->getNumChildren();
	}

	return 0;
}


PyObject * PySceneObject::mp_subscript(Object *self, PyObject *key)
{
	if (PySlice_Check(key))
	{
		Py_ssize_t start, stop, step, slicelength;
		if (PySlice_GetIndicesEx(key, mp_length(self), &start, &stop, &step, &slicelength) == 0)
		{
			PyObject *result = PyList_New(slicelength);

			Py_ssize_t index = 0;
			for (Py_ssize_t i = start; (i != stop) && (index < slicelength); i += step)
			{
				PyList_SetItem(result, index++, sq_item(self, i));
			}

			return result;
		}
	}

	if (PyLong_Check(key))
	{
		long i = PyLong_AsLong(key);
		Py_ssize_t index = (i < 0) ? mp_length(self) + i : i;

		return sq_item(self, index);
	}

	Py_INCREF(Py_None);
	return Py_None;
}


int PySceneObject::mp_ass_subscript(Object *self, PyObject *key, PyObject *value)
{
	if (PySlice_Check(key))
	{
		Py_ssize_t start, stop, step, slicelength;
		if (PySlice_GetIndicesEx(key, mp_length(self), &start, &stop, &step, &slicelength) == 0)
		{
			Py_ssize_t index = 0;
			Py_ssize_t removedChildren = 0;
			for (Py_ssize_t i = start; (i != stop) && (index < slicelength); i += step)
			{
				PyObject *item = 0;
				if (value)
				{
					if (!PySequence_Check(value))
					{
						PyErr_SetString(PyExc_TypeError, "must assign iterable to extended slice");
						return -1;
					}

					if (PySequence_Size(value) != slicelength)
					{
						char errMsg[100];
						sprintf(errMsg, "attempt to assign sequence of size %d to extended slice of size %d", (int) PySequence_Size(value), (int) slicelength);
						PyErr_SetString(PyExc_ValueError, errMsg);
						return -1;
					}

					item = PySequence_GetItem(value, index++);
				}

				sq_ass_item(self, i - removedChildren, item);

				if (!item)
					removedChildren++;
			}
		}
	}
	else if (PyLong_Check(key))
	{
		long i = PyLong_AsLong(key);
		Py_ssize_t index = (i < 0) ? mp_length(self) + i : i;

		return sq_ass_item(self, index, value);
	}

	return 0;
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

	return PyUnicode_FromString(n.getString());
}


PyObject* PySceneObject::sotype(Object* self)
{
	SbName n("");

	if (self->inventorObject)
	{
		n = self->inventorObject->getTypeId().getName();
	}

	return PyUnicode_FromString(n.getString());
}


PyObject* PySceneObject::check_type(Object* self, PyObject *args)
{
	long derived = 0;

	if (self->inventorObject)
	{
		char *typeName = 0;
		if (PyArg_ParseTuple(args, "s", &typeName))
		{
			derived = self->inventorObject->isOfType(SoType::fromName(SbName(typeName)));
		}
	}

	return PyBool_FromLong(derived);
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
	int enable = 0;
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
	char *name = 0, *value = 0;;
	if (self->inventorObject && PyArg_ParseTuple(args, "s|s", &name, &value))
	{
        if (!value)
        {
            value = name;
            setFields(self->inventorObject, value);
        }
        else
        {
            SoField *field = self->inventorObject->getField(name);
            if (field)
            {
                field->set(value);
            }
        }
	}

	Py_INCREF(Py_None);
	return Py_None;
}


PyObject* PySceneObject::get(Object *self, PyObject *args)
{
	char *name = 0;
	int createIfNeeded = true;
	if (self->inventorObject)
	{
		if (PyArg_ParseTuple(args, "|sp", &name, &createIfNeeded))
		{
            if (name)
            {
                // name given
                if (self->inventorObject->isOfType(SoBaseKit::getClassTypeId()))
                {
                    // return part?
                    SoBaseKit *baseKit = (SoBaseKit *) self->inventorObject;
                    if (baseKit->getNodekitCatalog()->getPartNumber(name) >= 0)
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
                    // return string value for field
                    SbString value;
                    field->get(value);
                    return PyUnicode_FromString(value.getString());
                }
            }
            else
            {
                // no name then return all field values that are not default
                if (self->inventorObject)
                {
                    SbString value;
                    self->inventorObject->get(value);
            
                    return PyUnicode_FromString(value.getString());
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
            PyObject* fieldNameType = PyTuple_New(3);
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

            PyObject *fieldWrapper = PyObject_CallObject((PyObject*)PyField::getType(), NULL);
            ((PyField*)fieldWrapper)->setInstance(fieldList[i]);
            PyTuple_SetItem(fieldNameType, 2, fieldWrapper);
            PyList_SetItem(result, i, fieldNameType);
		}
		return result;
	}
    
	Py_INCREF(Py_None);
	return Py_None;
}


PyObject* PySceneObject::getoutputs(Object* self)
{
	if (self->inventorObject)
	{
        SoEngineOutputList outputList;
        int numOutputs = ((SoEngine*)(self->inventorObject))->getOutputs(outputList);
        
        PyObject *result = PyList_New(numOutputs);
		for (int i = 0; i < numOutputs; ++i)
		{
            PyObject* outputNameType = PyTuple_New(3);
            SbName outputName("");
            if (((SoEngine*)(self->inventorObject))->getOutputName(outputList[i], outputName))
            {
                PyTuple_SetItem(outputNameType, 0, PyUnicode_FromString(outputName.getString()));
            }
            else
            {
                PyTuple_SetItem(outputNameType, 0, PyUnicode_FromString(""));
            }
            PyTuple_SetItem(outputNameType, 1, PyUnicode_FromString(outputList[i]->getConnectionType().getName().getString()));

            PyObject *outputWrapper = PyObject_CallObject((PyObject*)PyEngineOutput::getType(), NULL);
            ((PyEngineOutput*)outputWrapper)->setInstance(outputList[i]);
            PyTuple_SetItem(outputNameType, 2, outputWrapper);
            PyList_SetItem(result, i, outputNameType);
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

