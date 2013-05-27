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
#include "PySensor.h"

#pragma warning ( disable : 4127 ) // conditional expression is constant in Py_DECREF
#pragma warning ( disable : 4244 ) // possible loss of data when converting int to short in SbVec2s



PyTypeObject *PySensor::getType()
{
	static PyMemberDef members[] = 
	{
		{"callback", T_OBJECT_EX, offsetof(Object, callback), 0, "Sensor callback"},
		{NULL}  /* Sentinel */
	};

	static PyMethodDef methods[] = 
	{
		{"attach", (PyCFunction) attach, METH_VARARGS, "Attaches sensor to node or field" },
		{"detach", (PyCFunction) detach, METH_NOARGS, "Detaches sensor from node or field" },
		{"setinterval", (PyCFunction) setinterval, METH_VARARGS, "Sets a timer interval in which the sensor callback is invoked" },
		{"settime", (PyCFunction) settime, METH_VARARGS, "Sets a time from now at which the sensor callback is invoked" },
		{"schedule", (PyCFunction) schedule, METH_NOARGS, "Schedules a timer sensor" },
		{"unschedule", (PyCFunction) unschedule, METH_NOARGS, "Unschedules a timer sensor" },
		{"isscheduled", (PyCFunction) isscheduled, METH_NOARGS, "Returns true if a sensor is scheduled" },
		{NULL}  /* Sentinel */
	};

	static PyTypeObject managerType = 
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
		Py_TPFLAGS_BASETYPE,   /* tp_flags */
		"Sensor object",           /* tp_doc */
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


void PySensor::tp_dealloc(Object* self)
{
	if (self->sensor)
	{
		delete self->sensor;
	}

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
	}

	return (PyObject *) self;
}


int PySensor::tp_init(Object *self, PyObject * /*args*/, PyObject * /*kwds*/)
{
	Py_INCREF(Py_None);
	self->callback = Py_None;

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


PyObject* PySensor::attach(Object *self, PyObject *args)
{
	PyObject *node = 0;
	char *fieldName = 0;
	if (PyArg_ParseTuple(args, "O|s", &node, &fieldName))
	{
		SoFieldContainer *fc = 0;
		SoField *field = 0;

		if (node && PyNode_Check(node))
		{
			fc = ((PySceneObject::Object*) node)->inventorObject;
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

	Py_INCREF(Py_None);
	return Py_None;
}


PyObject* PySensor::setinterval(Object *self, PyObject *args)
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


PyObject* PySensor::settime(Object *self, PyObject *args)
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


PyObject* PySensor::isscheduled(Object *self)
{
	long val = 0;

	if (self->sensor)
	{
		val = self->sensor->isScheduled();
	}

	return PyBool_FromLong(val);
}

