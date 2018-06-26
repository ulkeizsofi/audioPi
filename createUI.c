#include <Python.h>
#include "utils.h"

void makelist(PyObject **effect_names, PyObject **effect_args, PyObject** limits);

effect_descriptor effect_descriptor_array;
int total_number_of_effects = 0;

int fd[2];



int createUI()
{
    PyObject *module_name, *module, *func;
    PyObject *arguments, *return_value;
    PyObject *effect_names, *effect_args, *limits, *py_pd;

    char* progname = "ui"; //python module name
    char* funcname = "print_to_ui"; //python function name

    int i;

    //Initialize the table of loaded modules
    //Create the fundamental modules __builtin__, __main__ and sys
    //Initialize the module search path (sys.path).
    Py_Initialize();
    
    //Run some simple python commands for initializing the path
    PyRun_SimpleString("import sys");
    PyRun_SimpleString("sys.path.append(\"/home/pi/alsasound/audioPi\")");

    //Create python string for module name
    module_name = PyString_FromString(progname);

    //Try to import the module
    module = PyImport_Import(module_name);
    Py_DECREF(module_name); //Not needed anymore

    if (module != NULL) {
        //Try to find the function in the module
        func = PyObject_GetAttrString(module, funcname);

        //Check if it was found and it is callable
        if (func && PyCallable_Check(func)) {
            
            //We must pass a tuple to the function
            //Initialize the tuple
            arguments = PyTuple_New(4);

            //Transform the C objects to Python objects
            makelist(&effect_names, &effect_args, &limits);
            if (!effect_names || !effect_args || !limits) { //Check if success
                Py_DECREF(arguments);
                Py_DECREF(module);
                fprintf(stderr, "Cannot convert argument\n");
                return 1;
            }

            //Fill the tuple with the python objects
            PyTuple_SetItem(arguments, 0, effect_names);
            PyTuple_SetItem(arguments, 1, effect_args);
            PyTuple_SetItem(arguments, 2, limits);
            py_pd = PyInt_FromLong(fd[1]);
            PyTuple_SetItem(arguments, 3, py_pd);

            //Call the function with the arguments
            return_value = PyObject_CallObject(func, arguments);
            Py_DECREF(arguments); //not needed anymore
            
            //Check return value
            if (return_value != NULL) {
                printf("Result of call: %ld\n", PyInt_AsLong(return_value));
                Py_DECREF(return_value);
            }
            else {
                Py_DECREF(func);
                Py_DECREF(module);
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
        Py_XDECREF(func);
        Py_DECREF(module);
    }
    else {
        PyErr_Print();
        fprintf(stderr, "Failed to load \"%s\"\n", progname);
        return 1;
    }
    //Free all the memory needed
    Py_Finalize();
    return 0;
}

void makelist(PyObject **effect_names, PyObject **effect_args, PyObject** limits) {
    *effect_names = PyList_New(total_number_of_effects);
    *effect_args = PyList_New(total_number_of_effects);
    *limits = PyList_New(total_number_of_effects);
    for (size_t i = 0; i != total_number_of_effects ; ++i) {
        PyList_SET_ITEM(*effect_names, i, PyString_FromString(effect_descriptor_array.names[i]));
        PyList_SET_ITEM(*effect_args, i, PyLong_FromLong(effect_descriptor_array.args[i]));
        PyObject* limlist = PyList_New(effect_descriptor_array.args[i]);
        for (int j = 0; j < effect_descriptor_array.args[i]; j++){
            PyObject* l = PyTuple_New(2);
            PyTuple_SetItem(l, 0, PyLong_FromDouble(effect_descriptor_array.lims[i][j].min));
            PyTuple_SetItem(l, 1, PyLong_FromDouble(effect_descriptor_array.lims[i][j].max));
            PyList_SET_ITEM(limlist, j, l);
        }
        PyList_SET_ITEM(*limits, i, limlist);
    }
}
