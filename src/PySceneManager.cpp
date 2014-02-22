/**
 * \file   
 * \brief      PySceneManager class implementation.
 * \author     Thomas Moeller
 * \details
 *
 * Copyright (C) the PyInventor contributors. All rights reserved.
 * This file is part of PyInventor, distributed under the BSD 3-Clause
 * License. For full terms see the included COPYING file.
 */


#include <Inventor/SoSceneManager.h>
#include <Inventor/events/SoLocation2Event.h>
#include <Inventor/events/SoMouseButtonEvent.h>
#include <Inventor/events/SoKeyboardEvent.h>
#include <Inventor/nodes/SoNode.h>
#include <Inventor/engines/SoEngine.h>
#include <Inventor/elements/SoGLLazyElement.h> // for GL.h, whose location is distribution specific under system/ or sys/
#include <Inventor/SoDB.h>

#ifdef TGS_VERSION
#include <Inventor/events/SoMouseWheelEvent.h>
#endif

#include "PySceneManager.h"

#pragma warning ( disable : 4127 ) // conditional expression is constant in Py_DECREF
#pragma warning ( disable : 4244 ) // possible loss of data when converting int to short in SbVec2s



PyTypeObject *PySceneManager::getType()
{
	static PyMemberDef members[] = 
	{
		{"scene", T_OBJECT_EX, offsetof(Object, scene), 0, "scene graph"},
		{"redisplay", T_OBJECT_EX, offsetof(Object, renderCallback), 0, "render callback"},
		{NULL}  /* Sentinel */
	};

	static PyMethodDef methods[] = 
	{
		{"render", (PyCFunction) render, METH_NOARGS, "Renders the scene into an OpenGL context" },
		{"resize", (PyCFunction) resize, METH_VARARGS, "Sets the window size" },
		{"mouse_button", (PyCFunction) mouse_button, METH_VARARGS, "Sends mouse button event into the scene for processing" },
		{"mouse_move", (PyCFunction) mouse_move, METH_VARARGS, "Sends mouse move event into the scene for processing" },
		{"key", (PyCFunction) key, METH_VARARGS, "Sends keyboard event into the scene for processing" },
		{NULL}  /* Sentinel */
	};

	static PyTypeObject managerType = 
	{
		PyVarObject_HEAD_INIT(NULL, 0)
		"SceneManager",            /* tp_name */
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
		(setattrofunc)tp_setattro, /* tp_setattro */
		0,                         /* tp_as_buffer */
		Py_TPFLAGS_DEFAULT |
			Py_TPFLAGS_BASETYPE,   /* tp_flags */
		"Scene manager object",    /* tp_doc */
		0,                         /* tp_traverse */
		0,                         /* tp_clear */
		0,                         /* tp_richcompare */
		0,                         /* tp_weaklistoffset */
		0,                         /* tp_iter */
		0,                         /* tp_iternext */
		methods,                   /* tp_methods */
		members,                   /* tp_members */
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

	return &managerType;
}


void PySceneManager::tp_dealloc(Object* self)
{
	if (self->sceneManager)
	{
		delete self->sceneManager;
	}

	Py_XDECREF(self->scene);
	Py_XDECREF(self->renderCallback);

    Py_TYPE(self)->tp_free((PyObject*)self);
}


PyObject* PySceneManager::tp_new(PyTypeObject *type, PyObject* /*args*/, PyObject* /*kwds*/)
{
	PySceneObject::initSoDB();

    Object *self = (Object *)type->tp_alloc(type, 0);
    if (self != NULL) 
	{
		self->scene = 0;
		self->renderCallback = 0;
		self->sceneManager = 0;
	}

    return (PyObject *) self;
}


int PySceneManager::tp_init(Object *self, PyObject * /*args*/, PyObject * /*kwds*/)
{
	self->scene = PySceneObject::createWrapper("Separator");
	self->sceneManager = new SoSceneManager();
	if (self->scene && self->sceneManager)
	{
		self->sceneManager->setSceneGraph((SoNode*) ((PySceneObject::Object*) self->scene)->inventorObject);
	}
	self->sceneManager->setRenderCallback(renderCBFunc, self);
	self->sceneManager->activate();

	Py_INCREF(Py_None);
	self->renderCallback = Py_None;

	return 0;
}


int PySceneManager::tp_setattro(Object* self, PyObject *attrname, PyObject *value)
{
	int result = 0;

	result = PyObject_GenericSetAttr((PyObject*) self, attrname, value);

	const char *attr = PyUnicode_AsUTF8(attrname);
	if (attr && (strcmp(attr, "scene") == 0))
	{
		if (PyNode_Check(self->scene))
		{
			SoFieldContainer *fc = ((PySceneObject::Object*) self->scene)->inventorObject;
			if (fc && fc->isOfType(SoNode::getClassTypeId()))
			{
				self->sceneManager->setSceneGraph((SoNode*) fc);
				self->sceneManager->scheduleRedraw();
			}
			else
			{
				PyErr_SetString(PyExc_IndexError, "Scene object must be of type SoNode");
			}
		}
		else
		{
			PyErr_SetString(PyExc_IndexError, "Scene must be of type Node");
		}
	}

	return result;
}


void PySceneManager::renderCBFunc(void *userdata, SoSceneManager * /*mgr*/)
{
	Object *self = (Object *) userdata;
	if ((self != NULL) && (self->renderCallback != NULL) && PyCallable_Check(self->renderCallback))
	{
		PyObject *value = PyObject_CallObject(self->renderCallback, NULL);
		if (value != NULL)
		{
			Py_DECREF(value);
		}
	}
}


PyObject* PySceneManager::render(Object *self)
{
	self->sceneManager->render();
    
    // need to flush or nothing will be shown on OS X
    glFlush();

    Py_INCREF(Py_None);
    return Py_None;
}


PyObject* PySceneManager::resize(Object *self, PyObject *args)
{
    int width = 0, height = 0;
    if (PyArg_ParseTuple(args, "ii", &width, &height))
	{
        // for Coin both setWindowSize() and setSize() must be called
        // in order to get correct rendering and event handling
		self->sceneManager->setWindowSize(SbVec2s(width, height));
        self->sceneManager->setSize(SbVec2s(width, height));
	}

    Py_INCREF(Py_None);
    return Py_None;
}


PyObject* PySceneManager::mouse_button(Object *self, PyObject *args)
{
    int button = 0, state = 0, x = 0, y = 0;
    if (PyArg_ParseTuple(args, "iiii", &button, &state, &x, &y))
	{
		y = self->sceneManager->getWindowSize()[1] - y;

        #ifdef TGS_VERSION
		// Coin does not have wheel event (yet)
        if (button > 2)
		{
			// wheel
			SoMouseWheelEvent ev;
			ev.setTime(SbTime::getTimeOfDay());
			ev.setDelta(state ? -120 : 120);
			if (self->sceneManager->processEvent(&ev))
			{
				SoDB::getSensorManager()->processDelayQueue(FALSE);
			}
		}
		else
        #endif
		{
			SoMouseButtonEvent ev;
			ev.setTime(SbTime::getTimeOfDay());
			ev.setPosition(SbVec2s(x, y));
			ev.setButton((SoMouseButtonEvent::Button) (button + 1));
			ev.setState(state ? SoMouseButtonEvent::UP : SoMouseButtonEvent::DOWN);
			if (self->sceneManager->processEvent(&ev))
			{
				SoDB::getSensorManager()->processDelayQueue(FALSE);
			}
		}
	}

    Py_INCREF(Py_None);
    return Py_None;
}


PyObject* PySceneManager::mouse_move(Object *self, PyObject *args)
{
    int x = 0, y = 0;
    if (PyArg_ParseTuple(args, "ii", &x, &y))
	{
		y = self->sceneManager->getWindowSize()[1] - y;

		SoLocation2Event ev;
		ev.setTime(SbTime::getTimeOfDay());
		ev.setPosition(SbVec2s(x, y));
		if (self->sceneManager->processEvent(&ev))
		{
			SoDB::getSensorManager()->processDelayQueue(FALSE);
		}
	}

    Py_INCREF(Py_None);
    return Py_None;
}


PyObject* PySceneManager::key(Object *self, PyObject *args)
{
    char key;
    if (PyArg_ParseTuple(args, "c", &key))
	{
		bool mapped = true;
		SoKeyboardEvent ev;
		ev.setTime(SbTime::getTimeOfDay());

		if ((key >= 'a') && (key <= 'z'))
		{
			ev.setKey(SoKeyboardEvent::Key(SoKeyboardEvent::A + (key - 'a')));
		}
		else if ((key >= 'A') && (key <= 'Z'))
		{
			ev.setKey(SoKeyboardEvent::Key(SoKeyboardEvent::A + (key - 'Z')));
			ev.setShiftDown(TRUE);
		}
		else switch (key)
		{
		case '\n':
		case '\r':		ev.setKey(SoKeyboardEvent::RETURN); break;
		case '\t':		ev.setKey(SoKeyboardEvent::TAB); break;
		case ' ':		ev.setKey(SoKeyboardEvent::SPACE); break;
		case ',':		ev.setKey(SoKeyboardEvent::COMMA); break;
		case '.':		ev.setKey(SoKeyboardEvent::PERIOD); break;
		case '=':		ev.setKey(SoKeyboardEvent::EQUAL); break;
		case '-':		ev.setKey(SoKeyboardEvent::PAD_SUBTRACT); break;
		case '+':		ev.setKey(SoKeyboardEvent::PAD_ADD); break;
		case '/':		ev.setKey(SoKeyboardEvent::PAD_DIVIDE); break;
		case '*':		ev.setKey(SoKeyboardEvent::PAD_MULTIPLY); break;
		case '\x1b':	ev.setKey(SoKeyboardEvent::ESCAPE); break;
		case '\x08':	ev.setKey(SoKeyboardEvent::BACKSPACE); break;
		default:
			mapped = false;
		}

		if (mapped)
		{
			ev.setState(SoButtonEvent::DOWN);
			SbBool processed = self->sceneManager->processEvent(&ev);
			ev.setState(SoButtonEvent::UP);
			processed |= self->sceneManager->processEvent(&ev);
			if (processed)
			{
				SoDB::getSensorManager()->processDelayQueue(FALSE);
			}
		}
	}

    Py_INCREF(Py_None);
    return Py_None;
}

