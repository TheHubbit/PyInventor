/**
 * \file   
 * \brief      PySensor class implementation.
 * \author     Thomas Moeller
 * \details
 *
 * Copyright (C) the PyInventor contributors. All rights reserved.
 * This file is part of PyInventor, distributed under the BSD 3-Clause
 * License. For full terms see the included COPYING file.
 */


#include <Inventor/sensors/SoSensors.h>
#include <Inventor/fields/SoFieldContainer.h>
#include <Inventor/nodes/SoSelection.h>
#include "PySensor.h"
#include "PyPath.h"

#pragma warning ( disable : 4127 ) // conditional expression is constant in Py_DECREF
#pragma warning ( disable : 4244 ) // possible loss of data when converting int to short in SbVec2s


PyTypeObject *PySensor::getType()
{
	static PyMemberDef members[] = 
	{
		{"callback", T_OBJECT_EX, offsetof(Object, callback), 0,
            "Sensor callback function.\n"
            "\n"
            "An object can be assigned to that property which is called when a\n"
            "sensor is triggered.\n"
        },
		{NULL}  /* Sentinel */
	};

	static PyMethodDef methods[] = 
	{
		{"attach", (PyCFunction) attach, METH_VARARGS,
            "Attaches sensor/callback to node or field. The callback is triggered\n"
            "when the attached node or field changes. If only a Node instance is\n"
            "given then the sensor triggers on any change of that node or children.\n"
            "If both a scene object instance and a field or callback name are passed,\n"
            "the sensor triggers on field changes only.\n"
            "For Selection nodes the names 'selection', 'deselection', 'start'\n"
            "'finish' are also accepted to register the corresponding callbacks.\n"
            "\n"
            "Args:\n"
            "    - node: Scene object instance to attach to.\n"
            "    - name: Optional field name (or callback for Selection node).\n"
            "    - func: The callback function can also be passed as argument\n"
            "            instead of assigning the callback property.\n"
        },
		{"detach", (PyCFunction) detach, METH_NOARGS,
            "Deactivates a sensor.\n"
        },
		{"set_interval", (PyCFunction) set_interval, METH_VARARGS,
            "Sets up a timer sensor with a regular interval.\n"
            "\n"
            "Args:\n"
            "    Timer interval in milliseconds.\n"
        },
		{"set_time", (PyCFunction) set_time, METH_VARARGS,
            "Sets up an alarm sensor that is trigegred at a given time from now.\n"
            "\n"
            "Args:\n"
            "    Timer from now in milliseconds when the sensor will be triggered.\n"
        },
		{"schedule", (PyCFunction) schedule, METH_NOARGS,
            "Schedules a timer sensor."
        },
		{"unschedule", (PyCFunction) unschedule, METH_NOARGS,
            "Unschedules a timer sensor."
        },
		{"is_scheduled", (PyCFunction) is_scheduled, METH_NOARGS,
            "Returns scheduled state of the sensor.\n"
            "\n"
            "Returns:\n"
            "    True is sensor is currently scheduled, otherwise False.\n"
        },
		{NULL}  /* Sentinel */
	};

	static PyTypeObject sensorType = 
	{
		PyVarObject_HEAD_INIT(NULL, 0)
		"Sensor",                  /* tp_name */
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
		0, //(setattrofunc)tp_setattro, /* tp_setattro */
		0,                         /* tp_as_buffer */
		Py_TPFLAGS_DEFAULT |
		Py_TPFLAGS_BASETYPE,       /* tp_flags */
        "Represents node, field, timer and alarm sensors.\n"
        "\n"
        "Sensors can be used to observe changes in scene graphs or to trigger actions\n"
        "at given times.\n",       /* tp_doc */
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

	return &sensorType;
}


void PySensor::unregisterSelectionCB(Object *self)
{
    if (self->selection)
    {
        switch (self->selectionCB)
        {
        case Object::CB_SELECTION: self->selection->removeSelectionCallback(selectionPathCB, self); break;
        case Object::CB_DESELECTION: self->selection->removeDeselectionCallback(selectionPathCB, self); break;
        case Object::CB_START: self->selection->removeStartCallback(selectionClassCB, self); break;
        case Object::CB_FINISH: self->selection->removeFinishCallback(selectionClassCB, self); break;
        }
        self->selection->unref();
        self->selection = 0;
    }
}


void PySensor::tp_dealloc(Object* self)
{
	if (self->sensor)
	{
		delete self->sensor;
	}

    unregisterSelectionCB(self);
    
	Py_XDECREF(self->callback);

	Py_TYPE(self)->tp_free((PyObject*)self);
}


PyObject* PySensor::tp_new(PyTypeObject *type, PyObject* /*args*/, PyObject* /*kwds*/)
{
	PySceneObject::initSoDB();

	Object *self = (Object *)type->tp_alloc(type, 0);
	if (self != NULL) 
	{
		self->callback = 0;
		self->sensor = 0;
        self->selection = 0;
        self->selectionCB = Object::CB_SELECTION;
    }

	return (PyObject *) self;
}


int PySensor::tp_init(Object *self, PyObject *args, PyObject * /*kwds*/)
{
	Py_INCREF(Py_None);
	self->callback = Py_None;

    attach(self, args);

	return 0;
}


void PySensor::sensorCBFunc(void *userdata, SoSensor* /*sensor*/)
{
	Object *self = (Object *) userdata;
	if ((self != NULL) && (self->callback != NULL) && PyCallable_Check(self->callback))
	{
		PyObject *value = PyObject_CallObject(self->callback, NULL);
		if (value != NULL)
		{
			Py_DECREF(value);
		}
	}
}


void PySensor::selectionPathCB(void * userdata, SoPath * path)
{
    Object *self = (Object *)userdata;
    if ((self != NULL) && (self->callback != NULL) && PyCallable_Check(self->callback))
    {
        PyObject *value = PyObject_CallObject(self->callback, Py_BuildValue("(O)", PyPath::createWrapper(path)));
        if (value != NULL)
        {
            Py_DECREF(value);
        }
    }
}


void PySensor::selectionClassCB(void * userdata, SoSelection * sel)
{
    Object *self = (Object *)userdata;
    if ((self != NULL) && (self->callback != NULL) && PyCallable_Check(self->callback))
    {
        PyObject *value = PyObject_CallObject(self->callback, Py_BuildValue("(O)", PySceneObject::createWrapper(sel)));
        if (value != NULL)
        {
            Py_DECREF(value);
        }
    }
}


PyObject* PySensor::attach(Object *self, PyObject *args)
{
	PyObject *node = 0, *callback = 0;
	char *fieldName = 0;
	if (PyArg_ParseTuple(args, "O|sO", &node, &fieldName, &callback))
	{
		SoFieldContainer *fc = 0;
		SoField *field = 0;

		if (node && PySceneObject_Check(node))
		{
			fc = ((PySceneObject::Object*) node)->inventorObject;
		}

        if (fc && fieldName && fc->isOfType(SoSelection::getClassTypeId()))
        {
            unregisterSelectionCB(self);

            if (SbString("selection") == fieldName)
            {
                self->selectionCB = Object::CB_SELECTION;
                self->selection = (SoSelection*) fc;
                self->selection->ref();
                self->selection->addSelectionCallback(selectionPathCB, self);
                fc = 0;
            }
            else if (SbString("deselection") == fieldName)
            {
                self->selectionCB = Object::CB_DESELECTION;
                self->selection = (SoSelection*) fc;
                self->selection->ref();
                self->selection->addDeselectionCallback(selectionPathCB, self);
                fc = 0;
            }
            else if (SbString("start") == fieldName)
            {
                self->selectionCB = Object::CB_START;
                self->selection = (SoSelection*) fc;
                self->selection->ref();
                self->selection->addStartCallback(selectionClassCB, self);
                fc = 0;
            }
            else if (SbString("finish") == fieldName)
            {
                self->selectionCB = Object::CB_FINISH;
                self->selection = (SoSelection*)fc;
                self->selection->ref();
                self->selection->addFinishCallback(selectionClassCB, self);
                fc = 0;
            }
        }

        if (fc && fieldName)
        {
            field = fc->getField(fieldName);
        }

		if ((fc && !fieldName) || field)
		{
			if (self->sensor)
			{
				delete self->sensor;
				self->sensor = 0;
			}

			if (field)
			{
				self->sensor = new SoFieldSensor(sensorCBFunc, self);
				((SoFieldSensor*) self->sensor)->attach(field);
			}
			else
			{
				self->sensor = new SoNodeSensor(sensorCBFunc, self);
				((SoNodeSensor*) self->sensor)->attach((SoNode*) fc);
			}
		}

        // set callback attribute based on option third argument
        if (callback)
        {
            Py_XDECREF(self->callback);
            self->callback = callback;
            Py_INCREF(self->callback);
        }
	}

	Py_INCREF(Py_None);
	return Py_None;
}


PyObject* PySensor::detach(Object *self)
{
	if (self->sensor)
	{
		delete self->sensor;
		self->sensor = 0;
	}

    unregisterSelectionCB(self);

	Py_INCREF(Py_None);
	return Py_None;
}


PyObject* PySensor::set_interval(Object *self, PyObject *args)
{
	double interval = 0.;
	if (PyArg_ParseTuple(args, "d", &interval))
	{
		if (self->sensor)
		{
			delete self->sensor;
			self->sensor = 0;
		}

		self->sensor = new SoTimerSensor(sensorCBFunc, self);
		((SoTimerSensor*) self->sensor)->setInterval(SbTime(interval));
	}

	Py_INCREF(Py_None);
	return Py_None;
}


PyObject* PySensor::set_time(Object *self, PyObject *args)
{
	double time = 0.;
	if (PyArg_ParseTuple(args, "d", &time))
	{
		if (self->sensor)
		{
			delete self->sensor;
			self->sensor = 0;
		}

		self->sensor = new SoAlarmSensor(sensorCBFunc, self);
		((SoAlarmSensor*) self->sensor)->setTimeFromNow(SbTime(time));
	}

	Py_INCREF(Py_None);
	return Py_None;
}


PyObject* PySensor::schedule(Object *self)
{
	if (self->sensor && !self->sensor->isScheduled())
	{
		self->sensor->schedule();
	}

	Py_INCREF(Py_None);
	return Py_None;
}


PyObject* PySensor::unschedule(Object *self)
{
	if (self->sensor && self->sensor->isScheduled())
	{
		self->sensor->unschedule();
	}

	Py_INCREF(Py_None);
	return Py_None;
}


PyObject* PySensor::is_scheduled(Object *self)
{
	long val = 0;

	if (self->sensor)
	{
		val = self->sensor->isScheduled();
	}

	return PyBool_FromLong(val);
}

