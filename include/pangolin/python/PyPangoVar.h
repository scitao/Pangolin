/* This file is part of the Pangolin Project.
 * http://github.com/stevenlovegrove/Pangolin
 *
 * Copyright (c) 2011 Steven Lovegrove
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include <Python.h>
#include <structmember.h>
#include <iomanip>
#include <pangolin/var/var.h>

namespace pangolin
{

PyObject* GetPangoVarAsPython(const std::string& name)
{
    VarState::VarStoreContainer::iterator i = VarState::I().vars.find(name);
    if(i != VarState::I().vars.end()) {
        VarValueGeneric* var = i->second;

        try{
            if( !strcmp(var->TypeId(), typeid(bool).name() ) ) {
                const bool val = Var<bool>(*var).Get();
                return PyBool_FromLong( val );
            }else if( !strcmp(var->TypeId(), typeid(short).name() ) ||
                      !strcmp(var->TypeId(), typeid(int).name() ) ||
                      !strcmp(var->TypeId(), typeid(long).name() ) ) {
                const long val = Var<long>(*var).Get();
                return PyLong_FromLong( val );
            }else if( !strcmp(var->TypeId(), typeid(double).name() ) ||
                      !strcmp(var->TypeId(), typeid(float).name() ) ) {
                const double val = Var<double>(*var).Get();
                return PyFloat_FromDouble(val);
            }else{
                const std::string val = var->str->Get();
                return PyString_FromString(val.c_str());
            }
        }catch(std::exception) {
        }
    }

    Py_RETURN_NONE;
}

void SetPangoVarFromPython(const std::string& name, PyObject* val)
{
    try{
        if(PyString_Check(val)) {
            pangolin::Var<std::string> pango_var(name);
            pango_var = PyString_AsString(val);
        }else if(PyInt_Check(val) ) {
            pangolin::Var<int> pango_var(name);
            pango_var = PyInt_AsLong(val);
        }else if(PyLong_Check(val)) {
            pangolin::Var<long> pango_var(name);
            pango_var = PyLong_AsLong(val);
        }else if(PyBool_Check(val)) {
            pangolin::Var<bool> pango_var(name);
            pango_var = (val == Py_True) ? true : false;
        }else if(PyFloat_Check(val)) {
            pangolin::Var<double> pango_var(name);
            pango_var = PyFloat_AsDouble(val);
        }else{
            PyUniqueObj pystr = PyObject_Repr(val);
            pangolin::Var<std::string> pango_var(name);
            pango_var = std::string(PyString_AsString(pystr));
        }
    }catch(std::exception e) {
        pango_print_error("%s\n", e.what());
    }
}

struct PyPangoVar {
    static PyTypeObject Py_type;
    PyObject_HEAD

    PyPangoVar(PyTypeObject *type)
        : ob_refcnt(1), ob_type(type)
    {
    }

    static void Py_dealloc(PyPangoVar* self)
    {
        delete self;
    }

    static PyObject * Py_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
    {
        PyPangoVar* self = new PyPangoVar(type);
        return (PyObject *)self;
    }

    static int Py_init(PyPangoVar *self, PyObject *args, PyObject *kwds)
    {
        char* cNamespace = 0;
        if (!PyArg_ParseTuple(args, "s", &cNamespace))
            return -1;

        self->ns = std::string(cNamespace);

        return 0;
    }

    static PyObject* Py_getattr(PyPangoVar *self, char* name)
    {
        const std::string prefix = self->ns + ".";
        const std::string full_name = self->ns.empty() ? name : prefix + std::string(name);

        if( !strcmp(name, "__call__") ||
                !strcmp(name, "__dict__") ||
                !strcmp(name, "__methods__") ||
                !strcmp(name, "__class__") )
        {
            // Default behaviour
            return PyObject_GenericGetAttr((PyObject*)self, PyString_FromString(name));
        } else if( !strcmp(name, "__members__") ) {
            const int nss = prefix.size();
            PyObject* l = PyList_New(0);
            for(const std::string& s : VarState::I().var_adds) {
                if(!s.compare(0, nss, prefix)) {
                    int dot = s.find_first_of('.', nss);
                    if(dot != std::string::npos) {
                        std::string val = s.substr(nss, dot - nss);
                        PyList_Append(l, PyString_FromString(val.c_str()));
                    }else{
                        std::string val = s.substr(nss);
                        PyList_Append(l, PyString_FromString(val.c_str()));
                    }
                }
            }

            return l;
        }else if( pangolin::VarState::I().Exists(full_name) ) {
            return GetPangoVarAsPython(full_name);
        }else{
            PyPangoVar* obj = (PyPangoVar*)PyPangoVar::Py_new(&PyPangoVar::Py_type,NULL,NULL);
            if(obj) {
                obj->ns = full_name;
                return PyObject_Init((PyObject *)obj,&PyPangoVar::Py_type);
            }
            return (PyObject *)obj;
        }

        Py_RETURN_NONE;
    }

    static int Py_setattr(PyPangoVar *self, char* name, PyObject* val)
    {
        const std::string full_name = self->ns.empty() ? name : self->ns + "." + std::string(name);
        SetPangoVarFromPython(full_name, val);
        return 0;
    }

    std::string ns;
};

 PyTypeObject PyPangoVar::Py_type = {
    PyObject_HEAD_INIT(NULL)
    0,                                        /* ob_size*/
    "pangolin.PangoVar",                      /* tp_name*/
    sizeof(PyPangoVar),                       /* tp_basicsize*/
    0,                                        /* tp_itemsize*/
    (destructor)PyPangoVar::Py_dealloc,       /* tp_dealloc*/
    0,                                        /* tp_print*/
    (getattrfunc)PyPangoVar::Py_getattr,      /* tp_getattr*/
    (setattrfunc)PyPangoVar::Py_setattr,      /* tp_setattr*/
    0,                                        /* tp_compare*/
    0,                                        /* tp_repr*/
    0,                                        /* tp_as_number*/
    0,                                        /* tp_as_sequence*/
    0,                                        /* tp_as_mapping*/
    0,                                        /* tp_hash */
    0,                                        /* tp_call*/
    0,                                        /* tp_str*/
    0,                                        /* tp_getattro*/
    0,                                        /* tp_setattro*/
    0,                                        /* tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags*/
    "PyPangoVar object",                      /* tp_doc */
    0,		                              /* tp_traverse */
    0,		                              /* tp_clear */
    0,		                              /* tp_richcompare */
    0,		                              /* tp_weaklistoffset */
    0,		                              /* tp_iter */
    0,		                              /* tp_iternext */
    0,                                        /* tp_methods */
    0,                                        /* tp_members */
    0,                                        /* tp_getset */
    0,                                        /* tp_base */
    0,                                        /* tp_dict */
    0,                                        /* tp_descr_get */
    0,                                        /* tp_descr_set */
    0,                                        /* tp_dictoffset */
    (initproc)PyPangoVar::Py_init,            /* tp_init */
    0,                                        /* tp_alloc */
    (newfunc)PyPangoVar::Py_new,              /* tp_new */
};

}
