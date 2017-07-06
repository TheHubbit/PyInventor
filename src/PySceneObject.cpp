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
#include <Inventor/manips/SoTransformManip.h>
#include <Inventor/manips/SoClipPlaneManip.h>
#include <Inventor/manips/SoDirectionalLightManip.h>
#include <Inventor/manips/SoPointLightManip.h>
#include <Inventor/manips/SoSpotLightManip.h>
#include <Inventor/nodes/SoShape.h>
#include <Inventor/nodes/SoCamera.h>
#include <Inventor/nodes/SoLight.h>
#include <Inventor/engines/SoEngines.h>
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
#include "PyPath.h"
#include "PyNodekitCatalog.h"

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


// isTransformable() and createTransformPath() are helper functions as 
// described in Manipulators chapter of the Inventor Mentor book

// Is this node of a type that is influenced by transforms?
static SbBool isTransformable(SoNode *myNode)
{
    if (myNode->isOfType(SoGroup::getClassTypeId())
        || myNode->isOfType(SoShape::getClassTypeId())
        || myNode->isOfType(SoCamera::getClassTypeId())
        || myNode->isOfType(SoLight::getClassTypeId()))
        return TRUE;
    else
        return FALSE;
}


//  Create a path to the transform node that affects the tail
//  of the input path.  Three possible cases:
//   [1] The path-tail is a node kit. Just ask the node kit for
//       a path to the part called "transform"
//   [2] The path-tail is NOT a group.  Search siblings of path
//       tail from right to left until you find a transform. If
//       none is found, or if another transformable object is 
//       found (shape,group,light,or camera), then insert a 
//       transform just to the left of the tail. This way, the 
//       manipulator only affects the selected object.
//   [3] The path-tail IS a group.  Search its children left to
//       right until a transform is found. If a transformable
//       node is found first, insert a transform just left of 
//       that node.  This way the manip will affect all nodes
//       in the group.
static SoPath * createTransformPath(SoPath *inputPath)
{
    int pathLength = inputPath->getLength();
    if (pathLength < 2) // Won't be able to get parent of tail
        return NULL;

    SoNode *tail = inputPath->getTail();

    // CASE 1: The tail is a node kit.
    // Nodekits have built in policy for creating parts.
    // The kit copies inputPath, then extends it past the 
    // kit all the way down to the transform. It creates the
    // transform if necessary.
    if (tail->isOfType(SoBaseKit::getClassTypeId())) {
        SoBaseKit *kit = (SoBaseKit *)tail;
        return kit->createPathToPart("transform", TRUE, inputPath);
    }

    SoTransform *editXf = NULL;
    SoGroup     *parent;
    SbBool      existedBefore = FALSE;

    // CASE 2: The tail is not a group.
    SbBool isTailGroup;
    isTailGroup = tail->isOfType(SoGroup::getClassTypeId());
    if (!isTailGroup) {
        // 'parent' is node above tail. Search under parent right
        // to left for a transform. If we find a 'movable' node
        // insert a transform just left of tail.  
        parent = (SoGroup *)inputPath->getNode(pathLength - 2);
        int tailIndx = parent->findChild(tail);

        for (int i = tailIndx; (i >= 0) && (editXf == NULL); i--) {
            SoNode *myNode = parent->getChild(i);
            if (myNode->isOfType(SoTransform::getClassTypeId()))
                editXf = (SoTransform *)myNode;
            else if (i != tailIndx && (isTransformable(myNode)))
                break;
        }
        if (editXf == NULL) {
            existedBefore = FALSE;
            editXf = new SoTransform;
            parent->insertChild(editXf, tailIndx);
        }
        else
            existedBefore = TRUE;
    }
    // CASE 3: The tail is a group.
    else {
        // Search the children from left to right for transform 
        // nodes. Stop the search if we come to a movable node
        // and insert a transform before it.
        parent = (SoGroup *)tail;
        int i;
        for (i = 0;
            (i < parent->getNumChildren()) && (editXf == NULL);
            i++) {
            SoNode *myNode = parent->getChild(i);
            if (myNode->isOfType(SoTransform::getClassTypeId()))
                editXf = (SoTransform *)myNode;
            else if (isTransformable(myNode))
                break;
        }
        if (editXf == NULL) {
            existedBefore = FALSE;
            editXf = new SoTransform;
            parent->insertChild(editXf, i);
        }
        else
            existedBefore = TRUE;
    }

    // Create 'pathToXform.' Copy inputPath, then make last
    // node be editXf.
    SoPath *pathToXform = NULL;
    pathToXform = inputPath->copy();
    pathToXform->ref();
    if (!isTailGroup) // pop off the last entry.
        pathToXform->pop();
    // add editXf to the end
    int xfIndex = parent->findChild(editXf);
    pathToXform->append(xfIndex);
    pathToXform->unrefNoDelete();

    return(pathToXform);
}


// reports all Inventor errors with PyErr_SetString()
static void inventorErrorCallback(const SoError *error, void * /*data*/)
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
		{"set_name", (PyCFunction) set_name, METH_VARARGS,
            "Sets the instance name of a scene object.\n"
            "\n"
            "Args:\n"
            "    Name for scene object instance.\n"
        },
		{"get_name", (PyCFunction) get_name, METH_NOARGS,
            "Returns the instance name of a scene object.\n"
            "\n"
            "Returns:\n"
            "    String containing scene object name.\n"
        },
		{"get_type", (PyCFunction) get_type, METH_NOARGS,
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
        { "get_field", (PyCFunction)get_field, METH_VARARGS,
            "Returns a field object by name or list of all fields.\n"
            "\n"
            "Returns:\n"
            "    Field matching the provided name or list of all fields if no name\n"
            "    was given.\n"
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
        "fields of each scene object type. Use the get_field() function to\n"
        "manage field connections, see also:\n"
        "- Field: Object representing a field.\n"
        "- EngineOutput: Object representing an engine output.\n"
        ,   /* tp_doc */
		0,                         /* tp_traverse */
		0,                         /* tp_clear */
        (richcmpfunc) tp_richcompare, /* tp_richcompare */
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
		0,                         /* tp_init */
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
		{"replace_node", (PyCFunction) replace_node, METH_VARARGS,
            "Replaces the node at the end of given path with this manipulator\n"
            "instance, which must be derived from SoTransformManip.\n"
            "\n"
            "Args:\n"
            "    Path to transform node that will be replaced with manipulator.\n"
        },
		{"replace_manip", (PyCFunction) replace_manip, METH_VARARGS,
            "Replaces this manipulator from the position identified by the\n"
            "given path with a transform node. This instance must be derived\n"
            "from SoTransformManip.\n"
            "\n"
            "Args:\n"
            "    Path to manipulator to be replaced and optionally instance of\n"
            "    transformation node to be inserted. If none is given an instance\n"
            "    of Transform will be created."
        },
		{"get_nodekit_catalog", (PyCFunction) get_nodekit_catalog, METH_NOARGS,
            "Returns catalog entries of a nodekit instance.\n"
            "\n"
            "Returns:\n"
            "    NodekitCatalog object, which is a list of dictionaries with\n"
            "    details about each nodekit part."
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
		0,                         /* tp_init */
		0,                         /* tp_alloc */
		tp_new,                    /* tp_new */
	};

	return &nodeType;
}


PyTypeObject *PySceneObject::getEngineType()
{
    static PyMethodDef methods[] =
    {
        { "get_output", (PyCFunction)get_output, METH_VARARGS,
            "Return the engine output by name or list of all outputs.\n"
            "\n"
            "Returns:\n"
            "    Engine output instance for provided name or list of all outputs if\n"
            "    no name is given.\n"
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
		0,                         /* tp_init */
		0,                         /* tp_alloc */
		tp_new,                    /* tp_new */
	};

	return &engineType;
}


PyTypeObject *PySceneObject::getWrapperType(const char *typeName)
{
	static WrapperTypes sceneObjectTypes;

    if (SbName("Node") == typeName)
    {
        return getNodeType();
    }
    else if (SbName("Engine") == typeName)
    {
        return getEngineType();
    }
    else if ((SbName("FieldContainer") == typeName) || (SbName("GlobalField") == typeName))
    {
        return getFieldContainerType();
    }

	if (sceneObjectTypes.find(typeName) == sceneObjectTypes.end())
	{
        SoType type = SoType::fromName(typeName);
        if (type.isBad())
        {
            return NULL;
        }

        PyTypeObject *baseType = getFieldContainerType();
        SoType parentType = type.getParent();
        if (parentType.canCreateInstance())
        {
            baseType = PySceneObject::getWrapperType(parentType.getName());
        }
        else if (parentType.isDerivedFrom(SoNode::getClassTypeId()))
        {
            baseType = getNodeType();
        }
        else if (parentType.isDerivedFrom(SoEngine::getClassTypeId()))
        {
            baseType = getEngineType();
        }

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
			(initproc) tp_init,        /* tp_init */
			0,                         /* tp_alloc */
			tp_new,                    /* tp_new */
		};
		sceneObjectTypes[typeName] = wrapperType;
	}

	return &sceneObjectTypes[typeName];
}


PyObject *PySceneObject::createWrapper(SoFieldContainer *instance, bool createClone)
{
    if (instance)
    {
        PyObject *obj = 0;

        SbName typeName = instance->getTypeId().getName();
        if (getWrapperType(typeName.getString()))
        {
            obj = PyObject_CallObject((PyObject*)getWrapperType(typeName.getString()), NULL);
        }
        else
        {
            PyObject* args = PyTuple_New(1);
            PyTuple_SetItem(args, 0, PyUnicode_FromString(typeName.getString()));
            obj = PyObject_CallObject((PyObject*)getNodeType(), args);
            Py_DECREF(args);
        }

        if (obj && instance && !createClone)
        {
            setInstance((Object*)obj, instance);
        }

        if (obj)
        {
            return obj;
        }
    }

    Py_INCREF(Py_None);
    return Py_None;
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
        if (kit->set(value))
        {
            // string contained part names and subgraphs
            return TRUE;
        }

        // proceed with setting fields if setting parts failed
	}

	return fieldContainer->set(value);
}


int PySceneObject::tp_init(Object *self, PyObject *args, PyObject *kwds)
{
	char *type = NULL, *name = NULL, *init = NULL;
	static char *kwlist[] = { "init", "name", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|ss", kwlist, &init, &name))
		return -1;

	if (self->inventorObject)
	{
		self->inventorObject->unref();
		self->inventorObject = 0;
	}

	PyObject *classObj = PyObject_GetAttrString((PyObject*) self, "__class__");
	if (classObj != NULL)
	{
		PyObject *className = PyObject_GetAttrString(classObj, "__name__");
		if (className != NULL)
		{
			// create new instance
			type = PyUnicode_AsUTF8(className);
			SoType t = SoType::fromName(type);

            // special handling for "templated" types (Gate, Concatenate, SelectOne)
            if (init && t.isDerivedFrom(SoGate::getClassTypeId()))
            {
                self->inventorObject = new SoGate(SoType::fromName(init));
                init = NULL;
            }
            else if (init && t.isDerivedFrom(SoConcatenate::getClassTypeId()))
            {
                self->inventorObject = new SoConcatenate(SoType::fromName(init));
                init = NULL;
            }
            else if (init && t.isDerivedFrom(SoSelectOne::getClassTypeId()))
            {
                self->inventorObject = new SoSelectOne(SoType::fromName(init));
                init = NULL;
            }
            else if (t.canCreateInstance())
            {
                self->inventorObject = (SoFieldContainer *)t.createInstance();
            }

            if (self->inventorObject)
			{
				self->inventorObject->ref();
				if (self->inventorObject->isOfType(SoEngine::getClassTypeId()) || 
					self->inventorObject->isOfType(SoNode::getClassTypeId()) )
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

		Py_DECREF(className);
	}
	Py_DECREF(classObj);
	
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
					PyObject *obj = createWrapper(node);
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
						PyObject *obj = createWrapper(node);
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
            if (!field->isOfType(SoSFTrigger::getClassTypeId()))
            {
                return PyField::setFieldValue(field, value);
            }
            else
            {
                field->touch();
            }
		}
	}

	return PyObject_GenericSetAttr((PyObject*) self, attrname, value);
}


PyObject *PySceneObject::tp_richcompare(Object *a, PyObject *b, int op)
{
    PyObject *returnObject = Py_NotImplemented;
    if (PySceneObject_Check(b))
    {
        bool result = false;
        void *otherInventorObject = ((Object*)b)->inventorObject;

        switch (op)
        {
        case Py_LT:	result = a->inventorObject < otherInventorObject; break;
        case Py_LE:	result = a->inventorObject <= otherInventorObject; break;
        case Py_EQ:	result = a->inventorObject == otherInventorObject; break;
        case Py_NE:	result = a->inventorObject != otherInventorObject; break;
        case Py_GT:	result = a->inventorObject > otherInventorObject; break;
        case Py_GE:	result = a->inventorObject >= otherInventorObject; break;
        }

        returnObject = result ? Py_True : Py_False;
    }

    Py_INCREF(returnObject);
    return  returnObject;
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
		PyObject *obj = createWrapper(self->inventorObject, true);
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
			SoNode *node = ((SoGroup*) self->inventorObject)->getChild(int(idx));
			PyObject *obj = createWrapper(node);
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
			((SoGroup*) self->inventorObject)->removeChild(int(idx));
			return 0;
		}
		else if (PySceneObject_Check(item))
		{
			if (((Object*) item)->inventorObject && ((Object*) item)->inventorObject->isOfType(SoNode::getClassTypeId()))
			{
				if (idx < ((SoGroup*) self->inventorObject)->getNumChildren())
				{
					// replace
					((SoGroup*) self->inventorObject)->replaceChild(int(idx), (SoNode*) ((Object*) item)->inventorObject);
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


PyObject* PySceneObject::set_name(Object* self, PyObject *args)
{
	char *name = 0;
	if (self->inventorObject && PyArg_ParseTuple(args, "s", &name))
	{
		self->inventorObject->setName(SbName(name));
	}

	Py_INCREF(Py_None);
	return Py_None;
}


PyObject* PySceneObject::get_name(Object* self)
{
	SbName n("");

	if (self->inventorObject)
	{
		n = self->inventorObject->getName();
	}

	return PyUnicode_FromString(n.getString());
}


PyObject* PySceneObject::get_type(Object* self)
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


PyObject* PySceneObject::set(Object *self, PyObject *args)
{
	char *name = 0, *value = 0;
	if (self->inventorObject && PyArg_ParseTuple(args, "s|s", &name, &value))
	{
        if (!value)
        {
            value = name;
            setFields(self->inventorObject, value);
        }
        else
        {
            SoNode *part = 0;

            if (self->inventorObject->isOfType(SoBaseKit::getClassTypeId()))
            {
                SoBaseKit *baseKit = (SoBaseKit *)self->inventorObject;
                if (baseKit->getNodekitCatalog()->getPartNumber(SbName(name)) != SO_CATALOG_NAME_NOT_FOUND)
                {
                    part = baseKit->getPart(name, TRUE);
                }
            }

            if (part)
            {
                part->set(value);
            }
            else
            {
                SoField *field = self->inventorObject->getField(name);
                if (field)
                {
                    if (!field->isOfType(SoSFTrigger::getClassTypeId()))
                    {
                        field->set(value);
                    }
                    else
                    {
                        field->touch();
                    }
                }
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
                            PyObject *obj = createWrapper(node);
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


PyObject* PySceneObject::get_field(Object* self, PyObject *args)
{
    char *name = 0;
    if (self->inventorObject && PyArg_ParseTuple(args, "|s", &name))
    {
        if (name)
        {
            // lookup field
            SoField *field = self->inventorObject->getField(SbName(name));
            if (field)
            {
                PyObject *fieldWrapper = PyObject_CallObject((PyObject*)PyField::getType(), NULL);
                ((PyField*)fieldWrapper)->setInstance(field);
                return fieldWrapper;
            }
            else
            {
                Py_INCREF(Py_None);
                return Py_None;
            }
        }
        else
        {
            // return all fields
            SoFieldList fieldList;
            int numFields = self->inventorObject->getFields(fieldList);

            PyObject *result = PyList_New(numFields);
            for (int i = 0; i < numFields; ++i)
            {
                PyObject *fieldWrapper = PyObject_CallObject((PyObject*)PyField::getType(), NULL);
                ((PyField*)fieldWrapper)->setInstance(fieldList[i]);
                PyList_SetItem(result, i, fieldWrapper);
            }
            return result;
        }
    }

    Py_INCREF(Py_None);
    return Py_None;
}


PyObject* PySceneObject::get_output(Object* self, PyObject *args)
{
    char *name = 0;
    if (self->inventorObject && PyArg_ParseTuple(args, "|s", &name))
    {
        if (name)
        {
            // lookup output
            SoEngineOutput *output = ((SoEngine*)self->inventorObject)->getOutput(SbName(name));
            if (output)
            {
                PyObject *outputWrapper = PyObject_CallObject((PyObject*)PyEngineOutput::getType(), NULL);
                ((PyEngineOutput*)outputWrapper)->setInstance(output);
                return outputWrapper;
            }
            else
            {
                Py_INCREF(Py_None);
                return Py_None;
            }
        }
        else
        {
            // return all outputs
            SoEngineOutputList outputList;
            int numOutputs = ((SoEngine*)(self->inventorObject))->getOutputs(outputList);

            PyObject *result = PyList_New(numOutputs);
            for (int i = 0; i < numOutputs; ++i)
            {
                PyObject *outputWrapper = PyObject_CallObject((PyObject*)PyEngineOutput::getType(), NULL);
                ((PyEngineOutput*)outputWrapper)->setInstance(outputList[i]);
                PyList_SetItem(result, i, outputWrapper);
            }
            return result;
        }
    }

    Py_INCREF(Py_None);
    return Py_None;
}


static void unref_fieldcontainer(PyObject* object) 
{
    ((SoFieldContainer*)PyCapsule_GetPointer(object, "SoFieldContainer"))->unref();
}


PyObject* PySceneObject::internal_pointer(Object* self)
{
	if (self->inventorObject)
	{
        self->inventorObject->ref();
        return PyCapsule_New((void*) self->inventorObject, "SoFieldContainer", unref_fieldcontainer);
	}

	Py_INCREF(Py_None);
	return Py_None;
}


PyObject* PySceneObject::replace_node(Object *self, PyObject *args)
{
    PyObject *pathObj = 0;
    if (self->inventorObject && PyArg_ParseTuple(args, "O", &pathObj))
    {
        if (PyObject_TypeCheck(pathObj, PyPath::getType()))
        {
            SoPath *path = PyPath::getInstance(pathObj);
            if (path)
            {
                if (self->inventorObject->isOfType(SoTransformManip::getClassTypeId()))
                {
                    SoPath *transformPath = createTransformPath(path);
                    transformPath->ref();
                    ((SoTransformManip*)self->inventorObject)->replaceNode(transformPath);
                    transformPath->unref();
                }
                if (self->inventorObject->isOfType(SoClipPlaneManip::getClassTypeId()))
                {
                    ((SoClipPlaneManip*)self->inventorObject)->replaceNode(path);
                }
                if (self->inventorObject->isOfType(SoDirectionalLightManip::getClassTypeId()))
                {
                    ((SoDirectionalLightManip*)self->inventorObject)->replaceNode(path);
                }
                if (self->inventorObject->isOfType(SoPointLightManip::getClassTypeId()))
                {
                    ((SoPointLightManip*)self->inventorObject)->replaceNode(path);
                }
                if (self->inventorObject->isOfType(SoSpotLightManip::getClassTypeId()))
                {
                    ((SoSpotLightManip*)self->inventorObject)->replaceNode(path);
                }
            }
        }
    }

    Py_INCREF(Py_None);
    return Py_None;
}


PyObject* PySceneObject::replace_manip(Object *self, PyObject *args)
{
    PyObject *pathObj = 0, *nodeObj = 0;
    if (self->inventorObject && PyArg_ParseTuple(args, "O|O", &pathObj, &nodeObj))
    {
        if (PyObject_TypeCheck(pathObj, PyPath::getType()))
        {
            SoPath *path = PyPath::getInstance(pathObj);
            SoFieldContainer *node = 0;

            if (nodeObj && PyNode_Check(nodeObj))
            {
                node = ((PySceneObject::Object*)nodeObj)->inventorObject;
                if (!node->isOfType(SoTransform::getClassTypeId()))
                {
                    // only SoTransform derived objects or NULL can be used
                    node = 0;
                }
            }

            if (path)
            {
                if (self->inventorObject->isOfType(SoTransformManip::getClassTypeId()))
                {
                    ((SoTransformManip*)self->inventorObject)->replaceManip(path, (SoTransform*)node);
                }
                if (self->inventorObject->isOfType(SoClipPlaneManip::getClassTypeId()))
                {
                    ((SoClipPlaneManip*)self->inventorObject)->replaceManip(path, (SoClipPlane*)node);
                }
                if (self->inventorObject->isOfType(SoDirectionalLightManip::getClassTypeId()))
                {
                    ((SoDirectionalLightManip*)self->inventorObject)->replaceManip(path, (SoDirectionalLight*)node);
                }
                if (self->inventorObject->isOfType(SoPointLightManip::getClassTypeId()))
                {
                    ((SoPointLightManip*)self->inventorObject)->replaceManip(path, (SoPointLight*)node);
                }
                if (self->inventorObject->isOfType(SoSpotLightManip::getClassTypeId()))
                {
                    ((SoSpotLightManip*)self->inventorObject)->replaceManip(path, (SoSpotLight*)node);
                }
            }
        }
    }

    Py_INCREF(Py_None);
    return Py_None;
}


PyObject* PySceneObject::get_nodekit_catalog(Object *self)
{
    PyObject *pathObj = 0, *nodeObj = 0;
    if (self->inventorObject && self->inventorObject->isOfType(SoBaseKit::getClassTypeId()))
    {
        return PyNodekitCatalog::createWrapper(((SoBaseKit*)self->inventorObject)->getNodekitCatalog());
    }

    Py_INCREF(Py_None);
    return Py_None;
}
