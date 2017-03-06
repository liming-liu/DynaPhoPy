#include <Python.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <complex.h>
#include <numpy/arrayobject.h>

#if defined(ENABLE_OPENMP)
#include <omp.h>
#endif


#undef I

static double EvaluateCorrelation (double AngularFrequency, double _Complex Velocity[], int NumberOfData, double TimeStep, int Increment, int IntMethod);
static double EvaluateCorrelation2 (double AngularFrequency, double _Complex Velocity[], int NumberOfData, double TimeStep, int Increment, int IntMethod);


// Correlation method for constant time (TimeStep) step trajectories paralellized with OpenMP
static PyObject* correlation_par (PyObject* self, PyObject *arg, PyObject *keywords)
{
    //  Declaring initial variables
    double  TimeStep;
    double  AngularFrequency;
 	int     Increment = 10;   //Default value for Increment
 	int     IntMethod = 1;    //Define integration method

    //  Interface with Python
    PyObject *velocity_obj, *frequency_obj;
    static char *kwlist[] = {"frequency", "velocity", "time_step", "step", "integration_method", NULL};
    if (!PyArg_ParseTupleAndKeywords(arg, keywords, "OOd|ii", kwlist, &frequency_obj, &velocity_obj, &TimeStep, &Increment, &IntMethod))  return NULL;

    PyObject *velocity_array = PyArray_FROM_OTF(velocity_obj, NPY_CDOUBLE, NPY_IN_ARRAY);
    PyObject *frequency_array = PyArray_FROM_OTF(frequency_obj, NPY_DOUBLE, NPY_IN_ARRAY);

    if (velocity_array == NULL || frequency_array == NULL ) {
        Py_XDECREF(velocity_array);
        Py_XDECREF(frequency_array);
        return NULL;
    }

    double _Complex  *Velocity = (double _Complex*)PyArray_DATA(velocity_array);
    double *Frequency    = (double*)PyArray_DATA(frequency_array);
    int     NumberOfData = (int)PyArray_DIM(velocity_array, 0);
    int     NumberOfFrequencies = (int)PyArray_DIM(frequency_array, 0);


    //Create new numpy array for storing result
    PyArrayObject *PowerSpectrum_object;
    int dims[1]={NumberOfFrequencies};
    PowerSpectrum_object = (PyArrayObject *) PyArray_FromDims(1,dims,NPY_DOUBLE);
    double *PowerSpectrum  = (double*)PyArray_DATA(PowerSpectrum_object);

    // Maximum Entropy Method Algorithm
    if (IntMethod < 0 || IntMethod > 1) {
        puts ("\nIntegration method selected does not exist\n");
        return NULL;
    }

# pragma omp parallel for default(shared) private(AngularFrequency)
    for (int i=0;i<NumberOfFrequencies;i++) {
        AngularFrequency = Frequency[i]*2.0*M_PI;
        PowerSpectrum[i] = EvaluateCorrelation(AngularFrequency, Velocity, NumberOfData, TimeStep, Increment, IntMethod);
    }
    //Returning Python array
    return(PyArray_Return(PowerSpectrum_object));
}

// Averaged
double EvaluateCorrelation (double Frequency, double _Complex Velocity[], int NumberOfData, double TimeStep, int Increment, int IntMethod) {
    double _Complex Correl = 0;
    for (int i = 0; i < NumberOfData; i += Increment) {
        for (int j = 0; j < (NumberOfData-i-Increment); j++) {
            Correl += conj(Velocity[j]) * Velocity[j+i] * cexp(_Complex_I*Frequency*(i*TimeStep));
            switch (IntMethod) {
                case 0: //	Trapezoid Integration
                    Correl += (conj(Velocity[j]) * Velocity[j+i+Increment] * cexp(_Complex_I*Frequency * ((i+Increment)*TimeStep))
                               +   conj(Velocity[j]) * Velocity[j+i]           * cexp(_Complex_I*Frequency * (i*TimeStep) ))/2.0 ;
                    break;
                case 1: //	Rectangular Integration
                    Correl +=  conj(Velocity[j]) * Velocity[j+i]          * cexp(_Complex_I*Frequency * (i*TimeStep));
                    break;
            }
        }
    }
    return  creal(Correl) * TimeStep/(NumberOfData/Increment);
}

// Original (simple)
double EvaluateCorrelation2 (double AngularFrequency, double _Complex Velocity[], int NumberOfData, double TimeStep, int Increment, int IntMethod) {

    double _Complex Correl;
    double _Complex Integral = 0;
    for (int i = 0; i < NumberOfData-Increment-1; i += Increment) {
        Correl = 0;
        for (int j = 0; j < (NumberOfData-i-Increment); j++) {


            switch (IntMethod) {
                    case 0: //	Trapezoid Integration
                    Correl += (conj(Velocity[j]) * Velocity[j+i+Increment] * cexp(_Complex_I*AngularFrequency * ((i+Increment)*TimeStep))
                               +   conj(Velocity[j]) * Velocity[j+i]           * cexp(_Complex_I*AngularFrequency * (i*TimeStep) ))/2.0 ;
                    break;
                    case 1: //	Rectangular Integration
                    Correl +=  conj(Velocity[j]) * Velocity[j+i]          * cexp(_Complex_I*AngularFrequency * (i*TimeStep));
                    break;
            }
         //   printf("\nDins %f",creal(Correl));

        }
        Integral += Correl/(NumberOfData -i -Increment);
      //  printf("\n%i %f",i, creal(Correl/(NumberOfData -i -Increment)));

    }


    return  creal(Integral)* TimeStep * Increment;
}

static char extension_docs_3[] =
    "correlation_par(frequency, velocity, timestep, step=10, integration_method=1 )\n\n Calculation of the correlation\n Constant time step method (faster)\n OpenMP parallel";

static PyMethodDef extension_funcs[] =
{
    {"correlation_par", (PyCFunction)correlation_par, METH_VARARGS|METH_KEYWORDS, extension_docs_3},
    {NULL}
};

void initcorrelation(void)
{
//  Importing numpy array types
    import_array();
    Py_InitModule3("correlation", extension_funcs,
                   "Fast Correlation Functions ");
};