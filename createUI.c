#include <Python.h>
#include "utils.h"

void makelist(PyObject **effectNames, PyObject **effectArgs, PyObject** limits);

effectDescriptor effectDescriptorArray;
int idx = 3;

int fd[2];



int createUI()
{
    PyObject *pName, *pModule, *pDict, *pFunc;
    PyObject *pArgs, *pValue;
    PyObject *effectNames, *effectArgs, *limits, *py_pd;


    char* progname = "ui";
    char* funcname = "print_to_ui";

    // char* array[] = {"hello", "Iam", "here"};
    int i;
    // printf("in createui %d\n", strlen(*array));

    Py_Initialize();
    PyRun_SimpleString("import sys");
    PyRun_SimpleString("sys.path.append(\"/home/pi/alsasound/audioPi\")");
    pName = PyString_FromString(progname);
    /* Error checking of pName left out */

    pModule = PyImport_Import(pName);
    Py_DECREF(pName);

    if (pModule != NULL) {
        pFunc = PyObject_GetAttrString(pModule, funcname);
        /* pFunc is a new reference */

        if (pFunc && PyCallable_Check(pFunc)) {
            pArgs = PyTuple_New(4);
            // for (i = 0; i < argc - 3; ++i) {
            makelist(&effectNames, &effectArgs, &limits);
            if (!effectNames || !effectArgs || !limits) {
                Py_DECREF(pArgs);
                Py_DECREF(pModule);
                fprintf(stderr, "Cannot convert argument\n");
                return 1;
            }
                /* pValue reference stolen here: */
            PyTuple_SetItem(pArgs, 0, effectNames);
            PyTuple_SetItem(pArgs, 1, effectArgs);
            PyTuple_SetItem(pArgs, 2, limits);
            // write(fd[1], "HERE", 5);
            printf("c: fd[1]: %d\n", fd[1]);
            py_pd = PyInt_FromLong(fd[1]);
            PyTuple_SetItem(pArgs, 3, py_pd);
            pValue = PyObject_CallObject(pFunc, pArgs);
            // }
            Py_DECREF(pArgs);
            if (pValue != NULL) {
                printf("Result of call: %ld\n", PyInt_AsLong(pValue));
                Py_DECREF(pValue);
            }
            else {
                Py_DECREF(pFunc);
                Py_DECREF(pModule);
                PyErr_Print();
                fprintf(stderr,"Call failed\n");
                return 1;
            }
        }
        else {
            if (PyErr_Occurred())
                PyErr_Print();
            fprintf(stderr, "Cannot find function \"%s\"\n", funcname);
        }
        Py_XDECREF(pFunc);
        Py_DECREF(pModule);
    }
    else {
        PyErr_Print();
        fprintf(stderr, "Failed to load \"%s\"\n", progname);
        return 1;
    }
    Py_Finalize();
    return 0;
}

void makelist(PyObject **effectNames, PyObject **effectArgs, PyObject** limits) {
    *effectNames = PyList_New(idx);
    *effectArgs = PyList_New(idx);
    *limits = PyList_New(idx);
    printf("%d\n", idx); 
    for (size_t i = 0; i != idx; ++i) {
        PyList_SET_ITEM(*effectNames, i, PyString_FromString(effectDescriptorArray.names[i]));
        PyList_SET_ITEM(*effectArgs, i, PyLong_FromLong(effectDescriptorArray.args[i]));
        PyObject* limlist = PyList_New(effectDescriptorArray.args[i]);
        for (int j = 0; j < effectDescriptorArray.args[i]; j++){
            PyObject* l = PyTuple_New(2);
            PyTuple_SetItem(l, 0, PyLong_FromDouble(effectDescriptorArray.lims[i][j].min));
            PyTuple_SetItem(l, 1, PyLong_FromDouble(effectDescriptorArray.lims[i][j].max));
            PyList_SET_ITEM(limlist, j, l);
        }
        PyList_SET_ITEM(*limits, i, limlist);
    }
}
