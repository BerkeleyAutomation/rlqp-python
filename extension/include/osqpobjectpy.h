#ifndef OSQPOBJECTPY_H
#define OSQPOBJECTPY_H

/****************************************
 * OSQP Object definition and methods   *
 ****************************************/

/* Create new OSQP Object */
static c_int OSQP_init(OSQP *self, PyObject *args, PyObject *kwds) {
	// OSQP *self;
	// self = PyObject_New(OSQP, &OSQP_Type);
	if (self == NULL)
		return -1;
	self->workspace = NULL;
	// return self;
	return 0;
}


// Deallocate OSQP object
static c_int OSQP_dealloc(OSQP *self) {
    // Cleanup workspace if not null
    if (self->workspace) {
        if (osqp_cleanup(self->workspace)) {
			PyErr_SetString(PyExc_ValueError, "Workspace deallocation error!");
			return 1;
		}
	}

    // Cleanup python object
    PyObject_Del(self);

    return 0;
}

// Solve Optimization Problem
static PyObject * OSQP_solve(OSQP *self) {
    c_int exitflag;

    // Create status object
    PyObject * status;

    // Create obj_val object
    PyObject * obj_val;

    // Create solution objects
    PyObject * x, *y, *prim_inf_cert, *dual_inf_cert;

    // Define info related variables
    static char *argparse_string;
    PyObject *info_list;
    PyObject *info;

    // Results
    PyObject *results_list;
    PyObject *results;

    npy_intp nd[1];
    npy_intp md[1];

    // Check that the workspace is initialized
    if (!self->workspace) {
        PyErr_SetString(PyExc_ValueError, "Workspace not initialized!");
        return (PyObject *) NULL;
    }

    // Temporary solution
    nd[0] = (npy_intp)self->workspace->data->n;  // Dimensions in R^n
    md[0] = (npy_intp)self->workspace->data->m;  // Dimensions in R^m

    /**
     *  Solve QP Problem
     */

    // Release the GIL
    Py_BEGIN_ALLOW_THREADS;
    exitflag = osqp_solve(self->workspace);
    Py_END_ALLOW_THREADS;

    if(exitflag){
        PyErr_SetString(PyExc_ValueError, "OSQP solve error!");
        return (PyObject *) NULL;
    }

    // If problem is not primal or dual infeasible store it
    if ((self->workspace->info->status_val != OSQP_PRIMAL_INFEASIBLE) &&
            (self->workspace->info->status_val != OSQP_PRIMAL_INFEASIBLE_INACCURATE) &&
            (self->workspace->info->status_val != OSQP_DUAL_INFEASIBLE) &&
            (self->workspace->info->status_val != OSQP_DUAL_INFEASIBLE_INACCURATE)){

        // Primal and dual solutions
        x = (PyObject *)PyArrayFromCArray(self->workspace->solution->x, nd);
        y = (PyObject *)PyArrayFromCArray(self->workspace->solution->y, md);

        // Infeasibility certificates -> None values
        prim_inf_cert = PyArray_EMPTY(1, nd, NPY_OBJECT, 0);
        dual_inf_cert = PyArray_EMPTY(1, md, NPY_OBJECT, 0);

    } else if (self->workspace->info->status_val == OSQP_PRIMAL_INFEASIBLE ||
            self->workspace->info->status_val == OSQP_PRIMAL_INFEASIBLE_INACCURATE) {
        // primal infeasible

        // Primal and dual solution arrays -> None values
        x = PyArray_EMPTY(1, nd, NPY_OBJECT, 0);
        y = PyArray_EMPTY(1, md, NPY_OBJECT, 0);

        // Primal infeasibility certificate
        prim_inf_cert = (PyObject *)PyArrayFromCArray(self->workspace->delta_y, md);

        // Dual infeasibility certificate -> None values
        dual_inf_cert = PyArray_EMPTY(1, nd, NPY_OBJECT, 0);

        // Set objective value to infinity
        self->workspace->info->obj_val = NPY_INFINITY;

    } else {
        // dual infeasible

        // Primal and dual solution arrays -> None values
        x = PyArray_EMPTY(1, nd, NPY_OBJECT, 0);
        y = PyArray_EMPTY(1, md, NPY_OBJECT, 0);

        // Primal infeasibility certificate -> None values
        prim_inf_cert = PyArray_EMPTY(1, md, NPY_OBJECT, 0);

        // Dual infeasibility certificate
        dual_inf_cert = (PyObject *)PyArrayFromCArray(self->workspace->delta_x, nd);

        // Set objective value to -infinity
        self->workspace->info->obj_val = -NPY_INFINITY;
    }

    /*  CREATE INFO OBJECT */
    // Store status string
    status = PyUnicode_FromString(self->workspace->info->status);

    // Store obj_val
    if (self->workspace->info->status_val == OSQP_NON_CVX) {	// non convex
        obj_val = PyFloat_FromDouble(Py_NAN);
    } else {
        obj_val = PyFloat_FromDouble(self->workspace->info->obj_val);
    }

#ifdef PROFILING

#ifdef DLONG

#ifdef DFLOAT
    argparse_string = "LOLLOfffffffLf";
#else
    argparse_string = "LOLLOdddddddLd";
#endif

#else

#ifdef DFLOAT
    argparse_string = "iOiiOfffffffif";
#else
    argparse_string = "iOiiOdddddddid";
#endif

#endif

    info_list = Py_BuildValue(argparse_string,
                    self->workspace->info->iter,
                    status,
                    self->workspace->info->status_val,
                    self->workspace->info->status_polish,
                    obj_val,
                    self->workspace->info->pri_res,
                    self->workspace->info->dua_res,
                    self->workspace->info->setup_time,
                    self->workspace->info->solve_time,
                    self->workspace->info->update_time,
                    self->workspace->info->polish_time,
                    self->workspace->info->run_time,
                    self->workspace->info->rho_updates,
                    self->workspace->info->rho_estimate
                    );
#else

#ifdef DLONG

#ifdef DFLOAT
    argparse_string = "LOLLOffLf";
#else
    argparse_string = "LOLLOddLd";
#endif

#else

#ifdef DFLOAT
    argparse_string = "iOiiOffif";
#else
    argparse_string = "iOiiOddid";
#endif

#endif

    info_list = Py_BuildValue(argparse_string,
            self->workspace->info->iter,
            status,
            self->workspace->info->status_val,
            self->workspace->info->status_polish,
            obj_val,
            self->workspace->info->pri_res,
            self->workspace->info->dua_res,
            self->workspace->info->rho_updates,
            self->workspace->info->rho_estimate,
            );
#endif

    info = PyObject_CallObject((PyObject *) &OSQP_info_Type, info_list);

    /* Release the info argument list. */
    Py_DECREF(info_list);

    /*  CREATE RESULTS OBJECT */
    results_list = Py_BuildValue("OOOOO", x, y, prim_inf_cert, dual_inf_cert, info);

    // /* Call the class object. */
    results = PyObject_CallObject((PyObject *) &OSQP_results_Type, results_list);

    // Return results
    Py_DECREF(results_list);
    return results;

}


// Setup optimization problem
static PyObject * OSQP_setup(OSQP *self, PyObject *args, PyObject *kwargs) {
    c_int n, m;  // Problem dimensions
    c_int exitflag;
	PyOSQPData *pydata;
	OSQPData * data;
	OSQPSettings * settings;

    PyArrayObject *Px, *Pi, *Pp, *q, *Ax, *Ai, *Ap, *l, *u;
    static char *kwlist[] = {"dims",                     // nvars and ncons
                             "Px", "Pi", "Pp", "q",      // Cost function
                             "Ax", "Ai", "Ap", "l", "u", // Constraints
                             "scaling",
                             "adaptive_rho", "adaptive_rho_interval",
                             "adaptive_rho_tolerance", "adaptive_rho_fraction",
                             "rho", "sigma", "max_iter", "eps_abs", "eps_rel",
                             "eps_prim_inf", "eps_dual_inf", "alpha", "delta",
                             "linsys_solver", "polish",
                             "polish_refine_iter", "verbose",
                             "scaled_termination",
                             "check_termination", "warm_start",
                             "time_limit", NULL};        // Settings

#ifdef DLONG

// NB: linsys_solver is enum type which is stored as int (regardless on how c_int is defined).

#ifdef DFLOAT
    static char * argparse_string = "(LL)O!O!O!O!O!O!O!O!O!|LLLffffLffffffiLLLLLLf";
#else
    static char * argparse_string = "(LL)O!O!O!O!O!O!O!O!O!|LLLddddLddddddiLLLLLLd";
#endif

#else

#ifdef DFLOAT
    static char * argparse_string = "(ii)O!O!O!O!O!O!O!O!O!|iiiffffiffffffiiiiiiif";
#else
    static char * argparse_string = "(ii)O!O!O!O!O!O!O!O!O!|iiiddddiddddddiiiiiiid";
#endif

#endif

    // Check that the workspace is not already initialized
    if (self->workspace) {
        PyErr_SetString(PyExc_ValueError, "Workspace already setup!");
        return (PyObject *) NULL;
    }

    // Initialize settings
    settings = (OSQPSettings *)c_malloc(sizeof(OSQPSettings));
	osqp_set_default_settings(settings);

    if( !PyArg_ParseTupleAndKeywords(args, kwargs, argparse_string, kwlist,
                                     &n, &m,
                                     &PyArray_Type, &Px,
                                     &PyArray_Type, &Pi,
                                     &PyArray_Type, &Pp,
                                     &PyArray_Type, &q,
                                     &PyArray_Type, &Ax,
                                     &PyArray_Type, &Ai,
                                     &PyArray_Type, &Ap,
                                     &PyArray_Type, &l,
                                     &PyArray_Type, &u,
                                     &settings->scaling,
                                     &settings->adaptive_rho,
                                     &settings->adaptive_rho_interval,
                                     &settings->adaptive_rho_tolerance,
                                     &settings->adaptive_rho_fraction,
                                     &settings->rho,
                                     &settings->sigma,
                                     &settings->max_iter,
                                     &settings->eps_abs,
                                     &settings->eps_rel,
                                     &settings->eps_prim_inf,
                                     &settings->eps_dual_inf,
                                     &settings->alpha,
                                     &settings->delta,
                                     &settings->linsys_solver,
                                     &settings->polish,
                                     &settings->polish_refine_iter,
                                     &settings->verbose,
                                     &settings->scaled_termination,
                                     &settings->check_termination,
                                     &settings->warm_start,
                                     &settings->time_limit)) {
        return (PyObject *) NULL;
    }

    // Create Data from parsed vectors
    pydata = create_pydata(n, m, Px, Pi, Pp, q, Ax, Ai, Ap, l, u);
    data = create_data(pydata);

    // Create Workspace object
    // Release the GIL
    Py_BEGIN_ALLOW_THREADS;
    exitflag = osqp_setup(&(self->workspace), data, settings);
    Py_END_ALLOW_THREADS;


    // Cleanup data and settings
    free_data(data, pydata);
    c_free(settings);

    if (!exitflag){ // Workspace allocation correct
        // Return workspace
        Py_INCREF(Py_None);
        return Py_None;
    } else {
        PyErr_SetString(PyExc_ValueError, "Workspace allocation error!");
        return (PyObject *) NULL;
    }
}


static PyObject *OSQP_version(OSQP *self) {
    return Py_BuildValue("s", osqp_version());
}


static PyObject *OSQP_dimensions(OSQP *self){

    // Check that the workspace is initialized
    if (!self->workspace) {
        PyErr_SetString(PyExc_ValueError, "Workspace not initialized!");
        return (PyObject *) NULL;
    }

#ifdef DLONG
    return Py_BuildValue("ll", self->workspace->data->n, self->workspace->data->m);
#else
    return Py_BuildValue("ii", self->workspace->data->n, self->workspace->data->m);
#endif
}




static PyObject *OSQP_update_lin_cost(OSQP *self, PyObject *args) {

    PyArrayObject *q, *q_cont;
    c_float * q_arr;
    int float_type = get_float_type();
    int exitflag = 0;

    static char * argparse_string = "O!";

    // Check that the workspace is initialized
    if (!self->workspace) {
        PyErr_SetString(PyExc_ValueError, "Workspace not initialized!");
        return (PyObject *) NULL;
    }

    // Parse arguments
    if( !PyArg_ParseTuple(args, argparse_string, &PyArray_Type, &q)) {
        return (PyObject *) NULL;
    }

    // Get contiguous data structure
    q_cont = get_contiguous(q, float_type);

    // Copy array into c_float array
    q_arr = (c_float *)PyArray_DATA(q_cont);

    // Update linear cost
    exitflag = osqp_update_lin_cost(self->workspace, q_arr);

    // Free data
    Py_DECREF(q_cont);

    if(exitflag){
        PyErr_SetString(PyExc_ValueError, "Linear cost update error!");
        return (PyObject *) NULL;
    }


    // Return None
    Py_INCREF(Py_None);
    return Py_None;

}

static PyObject *OSQP_update_lower_bound(OSQP *self, PyObject *args){

    PyArrayObject *l, *l_cont;
    c_float * l_arr;
    int float_type = get_float_type();
    int exitflag = 0;

    static char * argparse_string = "O!";

    // Check that the workspace is initialized
    if (!self->workspace) {
        PyErr_SetString(PyExc_ValueError, "Workspace not initialized!");
        return (PyObject *) NULL;
    }

    // Parse arguments
    if( !PyArg_ParseTuple(args, argparse_string, &PyArray_Type, &l)) {
        return (PyObject *) NULL;
    }

    // Get contiguous data structure
    l_cont = get_contiguous(l, float_type);

    // Copy array into c_float array
    l_arr = (c_float *)PyArray_DATA(l_cont);

    // Update lower bound
    exitflag = osqp_update_lower_bound(self->workspace, l_arr);

    // Free data
    Py_DECREF(l_cont);

    if(exitflag){
        PyErr_SetString(PyExc_ValueError, "Lower bound update error!");
        return (PyObject *) NULL;
    }

    // Return None
    Py_INCREF(Py_None);
    return Py_None;

}

static PyObject *OSQP_update_upper_bound(OSQP *self, PyObject *args){

    PyArrayObject *u, *u_cont;
    c_float * u_arr;
    int float_type = get_float_type();
    int exitflag = 0;

    static char * argparse_string = "O!";

    // Check that the workspace is initialized
    if (!self->workspace) {
        PyErr_SetString(PyExc_ValueError, "Workspace not initialized!");
        return (PyObject *) NULL;
    }

    // Parse arguments
    if( !PyArg_ParseTuple(args, argparse_string, &PyArray_Type, &u)) {
        return (PyObject *) NULL;
    }

    // Get contiguous data structure
    u_cont = get_contiguous(u, float_type);

    // Copy array into c_float array
    u_arr = (c_float *)PyArray_DATA(u_cont);

    // Update upper bound
    exitflag = osqp_update_upper_bound(self->workspace, u_arr);

    // Free data
    Py_DECREF(u_cont);

    if(exitflag){
        PyErr_SetString(PyExc_ValueError, "Upper bound update error!");
        return (PyObject *) NULL;
    }


    // Return None
    Py_INCREF(Py_None);
    return Py_None;
}


static PyObject *OSQP_update_bounds(OSQP *self, PyObject *args){

    PyArrayObject *l, *l_cont, *u, *u_cont;
    c_float * l_arr, * u_arr;
    int float_type = get_float_type();
    int exitflag = 0;

    static char * argparse_string = "O!O!";

    // Check that the workspace is initialized
    if (!self->workspace) {
        PyErr_SetString(PyExc_ValueError, "Workspace not initialized!");
        return (PyObject *) NULL;
    }

    // Parse arguments
    if( !PyArg_ParseTuple(args, argparse_string,
                          &PyArray_Type, &l,
                          &PyArray_Type, &u)) {
        return (PyObject *) NULL;
    }

    // Get contiguous data structure
    l_cont = get_contiguous(l, float_type);
    u_cont = get_contiguous(u, float_type);

    // Copy array into c_float array
    l_arr = (c_float *)PyArray_DATA(l_cont);
    u_arr = (c_float *)PyArray_DATA(u_cont);

    // Update bounds
    exitflag = osqp_update_bounds(self->workspace, l_arr, u_arr);

    // Free data
    Py_DECREF(l_cont);
    Py_DECREF(u_cont);

    if(exitflag){
        PyErr_SetString(PyExc_ValueError, "Bounds update error!");
        return (PyObject *) NULL;
    }


    // Return None
    Py_INCREF(Py_None);
    return Py_None;
}


// Update elements of matrix P
static PyObject * OSQP_update_P(OSQP *self, PyObject *args) {

    PyArrayObject *Px, *Px_cont, *Px_idx, *Px_idx_cont;
    c_float * Px_arr;
    c_int * Px_idx_arr;
    c_int Px_n;
    int exitflag = 0;
    int float_type = get_float_type();
    int int_type = get_int_type();

#ifdef DLONG
    static char * argparse_string = "O!O!L";
#else
    static char * argparse_string = "O!O!i";
#endif

    // Check that the workspace is initialized
    if (!self->workspace) {
        PyErr_SetString(PyExc_ValueError, "Workspace not initialized!");
        return (PyObject *) NULL;
    }

    // Parse arguments
    if( !PyArg_ParseTuple(args, argparse_string,
                          &PyArray_Type, &Px,
                          &PyArray_Type, &Px_idx,
                          &Px_n)) {
        return (PyObject *) NULL;
    }

    // Check if Px_idx is passed
    if (PyObject_Length((PyObject *)Px_idx) > 0) {
        Px_idx_cont = get_contiguous(Px_idx, int_type);
        Px_idx_arr = (c_int *)PyArray_DATA(Px_idx_cont);
    } else {
        Px_idx_cont = OSQP_NULL;
        Px_idx_arr = OSQP_NULL;
    }

    // Get contiguous data structure
    Px_cont = get_contiguous(Px, float_type);

    // Copy array into c_float and c_int arrays
    Px_arr = (c_float *)PyArray_DATA(Px_cont);

    // Update matrix P
    exitflag = osqp_update_P(self->workspace, Px_arr, Px_idx_arr, Px_n);

    // Free data
    Py_DECREF(Px_cont);
    if (PyObject_Length((PyObject *)Px_idx) > 0) Py_DECREF(Px_idx_cont);

    if(exitflag){
        PyErr_SetString(PyExc_ValueError, "P update error!");
        return (PyObject *) NULL;
    }


    // Return None
    Py_INCREF(Py_None);
    return Py_None;
}


// Update elements of matrix A
static PyObject * OSQP_update_A(OSQP *self, PyObject *args) {

    PyArrayObject *Ax, *Ax_cont, *Ax_idx, *Ax_idx_cont;
    c_float * Ax_arr;
    c_int * Ax_idx_arr;
    c_int Ax_n;
    int float_type = get_float_type();
    int int_type = get_int_type();
    int exitflag = 0;

#ifdef DLONG
    static char * argparse_string = "O!O!L";
#else
    static char * argparse_string = "O!O!i";
#endif

    // Check that the workspace is initialized
    if (!self->workspace) {
        PyErr_SetString(PyExc_ValueError, "Workspace not initialized!");
        return (PyObject *) NULL;
    }

	// Parse arguments
    if( !PyArg_ParseTuple(args, argparse_string,
                          &PyArray_Type, &Ax,
                          &PyArray_Type, &Ax_idx,
                          &Ax_n)) {
		return (PyObject *) NULL;
	}

	// Check if Ax_idx is passed
    if (PyObject_Length((PyObject *)Ax_idx) > 0) {
    Ax_idx_cont = get_contiguous(Ax_idx, int_type);
        Ax_idx_arr = (c_int *)PyArray_DATA(Ax_idx_cont);
    } else {
        Ax_idx_cont = OSQP_NULL;
        Ax_idx_arr = OSQP_NULL;
    }

    // Get contiguous data structure
    Ax_cont = get_contiguous(Ax, float_type);

    // Copy array into c_float and c_int arrays
    Ax_arr = (c_float *)PyArray_DATA(Ax_cont);

    // Update matrix A
    exitflag = osqp_update_A(self->workspace, Ax_arr, Ax_idx_arr, Ax_n);

    // Free data
    Py_DECREF(Ax_cont);
    if (PyObject_Length((PyObject *)Ax_idx) > 0) Py_DECREF(Ax_idx_cont);

    if(exitflag){
        PyErr_SetString(PyExc_ValueError, "A update error!");
        return (PyObject *) NULL;
    }

    // Return None
    Py_INCREF(Py_None);
    return Py_None;
}


// Update elements of matrices P and A
static PyObject * OSQP_update_P_A(OSQP *self, PyObject *args) {

    PyArrayObject *Px, *Px_cont, *Px_idx, *Px_idx_cont;
    PyArrayObject *Ax, *Ax_cont, *Ax_idx, *Ax_idx_cont;
    c_float * Px_arr, * Ax_arr;
    c_int * Px_idx_arr, * Ax_idx_arr;
    c_int Px_n, Ax_n;
    int float_type = get_float_type();
    int int_type = get_int_type();
    int exitflag = 0;

#ifdef DLONG
    static char * argparse_string = "O!O!LO!O!L";
#else
    static char * argparse_string = "O!O!iO!O!i";
#endif

    exitflag = 0;  // Assume successful execution

    // Check that the workspace is initialized
    if (!self->workspace) {
        PyErr_SetString(PyExc_ValueError, "Workspace not initialized!");
        return (PyObject *) NULL;
    }

	// Parse arguments
    if( !PyArg_ParseTuple(args, argparse_string,
                          &PyArray_Type, &Px,
                          &PyArray_Type, &Px_idx,
                          &Px_n,
                          &PyArray_Type, &Ax,
                          &PyArray_Type, &Ax_idx,
                          &Ax_n)) {
        return (PyObject *) NULL;
    }

	// Check if Ax_idx is passed
    if (PyObject_Length((PyObject *)Ax_idx) > 0) {
        Ax_idx_cont = get_contiguous(Ax_idx, int_type);
        Ax_idx_arr = (c_int *)PyArray_DATA(Ax_idx_cont);
    } else {
        Ax_idx_cont = OSQP_NULL;
        Ax_idx_arr = OSQP_NULL;
    }

    // Check if Px_idx is passed
    if (PyObject_Length((PyObject *)Px_idx) > 0) {
        Px_idx_cont = get_contiguous(Px_idx, int_type);
        Px_idx_arr = (c_int *)PyArray_DATA(Px_idx_cont);
    } else {
        Px_idx_cont = OSQP_NULL;
        Px_idx_arr = OSQP_NULL;
    }

    // Get contiguous data structure
    Px_cont = get_contiguous(Px, float_type);
    Ax_cont = get_contiguous(Ax, float_type);

    // Copy array into c_float and c_int arrays
    Px_arr = (c_float *)PyArray_DATA(Px_cont);
    Ax_arr = (c_float *)PyArray_DATA(Ax_cont);

    // Update matrices P and A
    exitflag = osqp_update_P_A(self->workspace,
                               Px_arr, Px_idx_arr, Px_n,
                               Ax_arr, Ax_idx_arr, Ax_n);

    // Free data
    Py_DECREF(Px_cont);
    if (PyObject_Length((PyObject *)Px_idx) > 0) Py_DECREF(Px_idx_cont);
    Py_DECREF(Ax_cont);
    if (PyObject_Length((PyObject *)Ax_idx) > 0) Py_DECREF(Ax_idx_cont);

    if(exitflag){
        PyErr_SetString(PyExc_ValueError, "P and A update error!");
        return (PyObject *) NULL;
    }

    // Return None
    Py_INCREF(Py_None);
    return Py_None;
}


static PyObject *OSQP_warm_start(OSQP *self, PyObject *args){

    PyArrayObject *x, *x_cont, *y, *y_cont;
    c_float * x_arr, * y_arr;
    int float_type = get_float_type();

    static char * argparse_string = "O!O!";

    // Check that the workspace is initialized
    if (!self->workspace) {
        PyErr_SetString(PyExc_ValueError, "Workspace not initialized!");
        return (PyObject *) NULL;
    }

    // Parse arguments
    if( !PyArg_ParseTuple(args, argparse_string,
                          &PyArray_Type, &x,
                          &PyArray_Type, &y)) {
        return (PyObject *) NULL;
    }

    // Get contiguous data structure
    x_cont = get_contiguous(x, float_type);
    y_cont = get_contiguous(y, float_type);

    // Copy array into c_float array
    x_arr = (c_float *)PyArray_DATA(x_cont);
    y_arr = (c_float *)PyArray_DATA(y_cont);

    // Update linear cost
    osqp_warm_start(self->workspace, x_arr, y_arr);

    // Free data
    Py_DECREF(x_cont);
    Py_DECREF(y_cont);

    // Return None
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *OSQP_warm_start_x(OSQP *self, PyObject *args) {

    PyArrayObject *x, *x_cont;
    c_float * x_arr;
    int float_type = get_float_type();

    static char * argparse_string = "O!";

    // Check that the workspace is initialized
    if (!self->workspace) {
        PyErr_SetString(PyExc_ValueError, "Workspace not initialized!");
        return (PyObject *) NULL;
    }

    // Parse arguments
    if( !PyArg_ParseTuple(args, argparse_string, &PyArray_Type, &x)) {
        return (PyObject *) NULL;
    }

    // Get contiguous data structure
    x_cont = get_contiguous(x, float_type);

    // Copy array into c_float array
    x_arr = (c_float *)PyArray_DATA(x_cont);

    // Update linear cost
    osqp_warm_start_x(self->workspace, x_arr);

    // Free data
    Py_DECREF(x_cont);

    // Return None
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *OSQP_warm_start_y(OSQP *self, PyObject *args) {

    PyArrayObject *y, *y_cont;
    c_float * y_arr;
    int float_type = get_float_type();

    static char * argparse_string = "O!";

    // Check that the workspace is initialized
    if (!self->workspace) {
        PyErr_SetString(PyExc_ValueError, "Workspace not initialized!");
        return (PyObject *) NULL;
    }

    // Parse arguments
    if( !PyArg_ParseTuple(args, argparse_string, &PyArray_Type, &y)) {
        return (PyObject *) NULL;
    }

    // Get contiguous data structure
    y_cont = get_contiguous(y, float_type);

    // Copy array into c_float array
    y_arr = (c_float *)PyArray_DATA(y_cont);

    // Update linear cost
    osqp_warm_start_y(self->workspace, y_arr);

    // Free data
    Py_DECREF(y_cont);

    // Return None
    Py_INCREF(Py_None);
    return Py_None;
}


static PyObject *OSQP_update_max_iter(OSQP *self, PyObject *args){

    c_int max_iter_new;

#ifdef DLONG
    static char * argparse_string = "L";
#else
    static char * argparse_string = "i";
#endif

    // Check that the workspace is initialized
    if (!self->workspace) {
        PyErr_SetString(PyExc_ValueError, "Workspace not initialized!");
        return (PyObject *) NULL;
    }

    // Parse arguments
    if( !PyArg_ParseTuple(args, argparse_string, &max_iter_new)) {
        return (PyObject *) NULL;
    }

    // Perform Update
    osqp_update_max_iter(self->workspace, max_iter_new);

    // Return None
    Py_INCREF(Py_None);
    return Py_None;
}


static PyObject *OSQP_update_eps_abs(OSQP *self, PyObject *args){

    c_float eps_abs_new;

#ifdef DFLOAT
    static char * argparse_string = "f";
#else
    static char * argparse_string = "d";
#endif

    // Check that the workspace is initialized
    if (!self->workspace) {
        PyErr_SetString(PyExc_ValueError, "Workspace not initialized!");
        return (PyObject *) NULL;
    }

    // Parse arguments
    if( !PyArg_ParseTuple(args, argparse_string, &eps_abs_new)) {
        return (PyObject *) NULL;
    }

    // Perform Update
    osqp_update_eps_abs(self->workspace, eps_abs_new);

    // Return None
    Py_INCREF(Py_None);
    return Py_None;
}


static PyObject *OSQP_update_eps_rel(OSQP *self, PyObject *args) {

    c_float eps_rel_new;

#ifdef DFLOAT
    static char * argparse_string = "f";
#else
    static char * argparse_string = "d";
#endif

    // Check that the workspace is initialized
    if (!self->workspace) {
        PyErr_SetString(PyExc_ValueError, "Workspace not initialized!");
        return (PyObject *) NULL;
    }

    // Parse arguments
    if( !PyArg_ParseTuple(args, argparse_string, &eps_rel_new)) {
        return (PyObject *) NULL;
    }

    // Perform Update
    osqp_update_eps_rel(self->workspace, eps_rel_new);

    // Return None
    Py_INCREF(Py_None);
    return Py_None;
}


static PyObject *OSQP_update_eps_prim_inf(OSQP *self, PyObject *args) {

    c_float eps_prim_inf_new;

#ifdef DFLOAT
    static char * argparse_string = "f";
#else
    static char * argparse_string = "d";
#endif

    // Check that the workspace is initialized
    if (!self->workspace) {
        PyErr_SetString(PyExc_ValueError, "Workspace not initialized!");
        return (PyObject *) NULL;
    }

    // Parse arguments
    if( !PyArg_ParseTuple(args, argparse_string, &eps_prim_inf_new)) {
        return (PyObject *) NULL;
    }

    // Perform Update
    osqp_update_eps_prim_inf(self->workspace, eps_prim_inf_new);

    // Return None
    Py_INCREF(Py_None);
    return Py_None;
}


static PyObject *OSQP_update_eps_dual_inf(OSQP *self, PyObject *args) {

    c_float eps_dual_inf_new;

#ifdef DFLOAT
    static char * argparse_string = "f";
#else
    static char * argparse_string = "d";
#endif

    // Check that the workspace is initialized
    if (!self->workspace) {
        PyErr_SetString(PyExc_ValueError, "Workspace not initialized!");
        return (PyObject *) NULL;
    }

    // Parse arguments
    if( !PyArg_ParseTuple(args, argparse_string, &eps_dual_inf_new)) {
        return (PyObject *) NULL;
    }

    // Perform Update
    osqp_update_eps_dual_inf(self->workspace, eps_dual_inf_new);

    // Return None
    Py_INCREF(Py_None);
    return Py_None;
}


static PyObject *OSQP_update_rho(OSQP *self, PyObject *args) {

    c_float rho_new;
    int exitflag = 0;

#ifdef DFLOAT
    static char * argparse_string = "f";
#else
    static char * argparse_string = "d";
#endif

    // Check that the workspace is initialized
    if (!self->workspace) {
        PyErr_SetString(PyExc_ValueError, "Workspace not initialized!");
        return (PyObject *) NULL;
    }

    // Parse arguments
    if( !PyArg_ParseTuple(args, argparse_string, &rho_new)) {
        return (PyObject *) NULL;
    }

    // Perform Update
    exitflag = osqp_update_rho(self->workspace, rho_new);

    if (exitflag){
        PyErr_SetString(PyExc_ValueError, "rho update error!");
        return (PyObject *) NULL;
    }


    // Return None
    Py_INCREF(Py_None);
    return Py_None;
}


static PyObject *OSQP_update_alpha(OSQP *self, PyObject *args) {

    c_float alpha_new;

#ifdef DFLOAT
    static char * argparse_string = "f";
#else
    static char * argparse_string = "d";
#endif

    // Check that the workspace is initialized
    if (!self->workspace) {
        PyErr_SetString(PyExc_ValueError, "Workspace not initialized!");
        return (PyObject *) NULL;
    }

    // Parse arguments
    if( !PyArg_ParseTuple(args, argparse_string, &alpha_new)) {
        return (PyObject *) NULL;
    }

    // Perform Update
    osqp_update_alpha(self->workspace, alpha_new);

    // Return None
    Py_INCREF(Py_None);
    return Py_None;
}


static PyObject *OSQP_update_delta(OSQP *self, PyObject *args){

    c_float delta_new;

#ifdef DFLOAT
    static char * argparse_string = "f";
#else
    static char * argparse_string = "d";
#endif

    // Check that the workspace is initialized
    if (!self->workspace) {
        PyErr_SetString(PyExc_ValueError, "Workspace not initialized!");
        return (PyObject *) NULL;
    }

    // Parse arguments
    if( !PyArg_ParseTuple(args, argparse_string, &delta_new)) {
        return (PyObject *) NULL;
    }

    // Perform Update
    osqp_update_delta(self->workspace, delta_new);

    // Return None
    Py_INCREF(Py_None);
    return Py_None;
}


static PyObject *OSQP_update_polish(OSQP *self, PyObject *args){

    c_int polish_new;

#ifdef DLONG
    static char * argparse_string = "L";
#else
    static char * argparse_string = "i";
#endif

    // Check that the workspace is initialized
    if (!self->workspace) {
        PyErr_SetString(PyExc_ValueError, "Workspace not initialized!");
        return (PyObject *) NULL;
    }

    // Parse arguments
    if( !PyArg_ParseTuple(args, argparse_string, &polish_new)) {
        return (PyObject *) NULL;
    }

    // Perform Update
    osqp_update_polish(self->workspace, polish_new);

    // Return None
    Py_INCREF(Py_None);
    return Py_None;
}


static PyObject *OSQP_update_polish_refine_iter(OSQP *self, PyObject *args){

    c_int polish_refine_iter_new;

#ifdef DLONG
    static char * argparse_string = "L";
#else
    static char * argparse_string = "i";
#endif

    // Check that the workspace is initialized
    if (!self->workspace) {
        PyErr_SetString(PyExc_ValueError, "Workspace not initialized!");
        return (PyObject *) NULL;
    }

    // Parse arguments
    if( !PyArg_ParseTuple(args, argparse_string, &polish_refine_iter_new)) {
        return (PyObject *) NULL;
    }

    // Perform Update
    osqp_update_polish_refine_iter(self->workspace, polish_refine_iter_new);

    // Return None
    Py_INCREF(Py_None);
    return Py_None;
}


static PyObject *OSQP_update_verbose(OSQP *self, PyObject *args){

    c_int verbose_new;

#ifdef DLONG
    static char * argparse_string = "L";
#else
    static char * argparse_string = "i";
#endif

    // Check that the workspace is initialized
    if (!self->workspace) {
        PyErr_SetString(PyExc_ValueError, "Workspace not initialized!");
        return (PyObject *) NULL;
    }

    // Parse arguments
    if( !PyArg_ParseTuple(args, argparse_string, &verbose_new)) {
        return (PyObject *) NULL;
    }

    // Perform Update
    osqp_update_verbose(self->workspace, verbose_new);

    // Return None
    Py_INCREF(Py_None);
    return Py_None;
}


static PyObject *OSQP_update_scaled_termination(OSQP *self, PyObject *args){

    c_int scaled_termination_new;

#ifdef DLONG
    static char * argparse_string = "L";
#else
    static char * argparse_string = "i";
#endif

    // Check that the workspace is initialized
    if (!self->workspace) {
        PyErr_SetString(PyExc_ValueError, "Workspace not initialized!");
        return (PyObject *) NULL;
    }

    // Parse arguments
    if( !PyArg_ParseTuple(args, argparse_string, &scaled_termination_new)) {
        return (PyObject *) NULL;
    }

    // Perform Update
    osqp_update_scaled_termination(self->workspace, scaled_termination_new);

    // Return None
    Py_INCREF(Py_None);
    return Py_None;
}


static PyObject *OSQP_update_check_termination(OSQP *self, PyObject *args){

    c_int check_termination_new;

#ifdef DLONG
    static char * argparse_string = "L";
#else
    static char * argparse_string = "i";
#endif

    // Check that the workspace is initialized
    if (!self->workspace) {
        PyErr_SetString(PyExc_ValueError, "Workspace not initialized!");
        return (PyObject *) NULL;
    }

    // Parse arguments
    if( !PyArg_ParseTuple(args, argparse_string, &check_termination_new)) {
        return (PyObject *) NULL;
    }

    // Perform Update
    osqp_update_check_termination(self->workspace, check_termination_new);

    // Return None
    Py_INCREF(Py_None);
    return Py_None;
}


static PyObject *OSQP_update_warm_start(OSQP *self, PyObject *args){

    c_int warm_start_new;

#ifdef DLONG
    static char * argparse_string = "L";
#else
    static char * argparse_string = "i";
#endif

    // Check that the workspace is initialized
    if (!self->workspace) {
        PyErr_SetString(PyExc_ValueError, "Workspace not initialized!");
        return (PyObject *) NULL;
    }

    // Parse arguments
    if( !PyArg_ParseTuple(args, argparse_string, &warm_start_new)) {
        return (PyObject *) NULL;
    }

    // Perform Update
    osqp_update_warm_start(self->workspace, warm_start_new);

    // Return None
    Py_INCREF(Py_None);
    return Py_None;
}


static PyObject *OSQP_update_time_limit(OSQP *self, PyObject *args){

    c_float time_limit_new;

#ifdef DFLOAT
    static char * argparse_string = "f";
#else
    static char * argparse_string = "d";
#endif

    // Check that the workspace is initialized
    if (!self->workspace) {
        PyErr_SetString(PyExc_ValueError, "Workspace not initialized!");
        return (PyObject *) NULL;
    }

    // Parse arguments
    if( !PyArg_ParseTuple(args, argparse_string, &time_limit_new)) {
        return (PyObject *) NULL;
    }

    // Perform Update
    osqp_update_time_limit(self->workspace, time_limit_new);

    // Return None
    Py_INCREF(Py_None);
    return Py_None;
}


static PyMethodDef OSQP_methods[] = {
    {"setup", (PyCFunction)OSQP_setup,METH_VARARGS|METH_KEYWORDS, PyDoc_STR("Setup OSQP problem")},
    {"solve", (PyCFunction)OSQP_solve, METH_VARARGS, PyDoc_STR("Solve OSQP problem")},
    {"version",	(PyCFunction)OSQP_version, METH_NOARGS, PyDoc_STR("OSQP version")},
    {"dimensions", (PyCFunction)OSQP_dimensions, METH_NOARGS, PyDoc_STR("Return problem dimensions (n, m)")},
    {"update_lin_cost",	(PyCFunction)OSQP_update_lin_cost, METH_VARARGS, PyDoc_STR("Update OSQP problem linear cost")},
    {"update_lower_bound", (PyCFunction)OSQP_update_lower_bound, METH_VARARGS, PyDoc_STR("Update OSQP problem lower bound")},
    {"update_upper_bound", (PyCFunction)OSQP_update_upper_bound, METH_VARARGS, PyDoc_STR("Update OSQP problem upper bound")},
    {"update_bounds", (PyCFunction)OSQP_update_bounds, METH_VARARGS, PyDoc_STR("Update OSQP problem bounds")},
	{"update_P", (PyCFunction)OSQP_update_P, METH_VARARGS, PyDoc_STR("Update OSQP problem quadratic cost matrix")},
	{"update_P_A", (PyCFunction)OSQP_update_P_A, METH_VARARGS, PyDoc_STR("Update OSQP problem matrices")},
	{"update_A", (PyCFunction)OSQP_update_A, METH_VARARGS, PyDoc_STR("Update OSQP problem constraint matrix")},
    {"warm_start", (PyCFunction)OSQP_warm_start, METH_VARARGS, PyDoc_STR("Warm start primal and dual variables")},
    {"warm_start_x", (PyCFunction)OSQP_warm_start_x, METH_VARARGS, PyDoc_STR("Warm start primal variable")},
    {"warm_start_y", (PyCFunction)OSQP_warm_start_y, METH_VARARGS, PyDoc_STR("Warm start dual variable")},
    {"update_max_iter", (PyCFunction)OSQP_update_max_iter, METH_VARARGS, PyDoc_STR("Update OSQP solver setting max_iter")},
    {"update_eps_abs", (PyCFunction)OSQP_update_eps_abs, METH_VARARGS, PyDoc_STR("Update OSQP solver setting eps_abs")},
    {"update_eps_rel", (PyCFunction)OSQP_update_eps_rel, METH_VARARGS, PyDoc_STR("Update OSQP solver setting eps_rel")},
    {"update_eps_prim_inf", (PyCFunction)OSQP_update_eps_prim_inf, METH_VARARGS, PyDoc_STR("Update OSQP solver setting eps_prim_inf")},
    {"update_eps_dual_inf",	(PyCFunction)OSQP_update_eps_dual_inf, METH_VARARGS, PyDoc_STR("Update OSQP solver setting eps_dual_inf")},
    {"update_alpha", (PyCFunction)OSQP_update_alpha, METH_VARARGS, PyDoc_STR("Update OSQP solver setting alpha")},
    {"update_rho", (PyCFunction)OSQP_update_rho, METH_VARARGS, PyDoc_STR("Update OSQP solver setting rho")},
    {"update_delta", (PyCFunction)OSQP_update_delta, METH_VARARGS, PyDoc_STR("Update OSQP solver setting delta")},
    {"update_polish", (PyCFunction)OSQP_update_polish, METH_VARARGS, PyDoc_STR("Update OSQP solver setting polish")},
    {"update_polish_refine_iter", (PyCFunction)OSQP_update_polish_refine_iter, METH_VARARGS, PyDoc_STR("Update OSQP solver setting polish_refine_iter")},
    {"update_verbose", (PyCFunction)OSQP_update_verbose, METH_VARARGS, PyDoc_STR("Update OSQP solver setting verbose")},
    {"update_scaled_termination", (PyCFunction)OSQP_update_scaled_termination, METH_VARARGS, PyDoc_STR("Update OSQP solver setting scaled_termination")},
    {"update_check_termination", (PyCFunction)OSQP_update_check_termination, METH_VARARGS, PyDoc_STR("Update OSQP solver setting check_termination")},
    {"update_warm_start", (PyCFunction)OSQP_update_warm_start, METH_VARARGS, PyDoc_STR("Update OSQP solver setting warm_start")},
    {"update_time_limit", (PyCFunction)OSQP_update_time_limit, METH_VARARGS, PyDoc_STR("Update OSQP solver setting time_limit")},
    {"_get_workspace", (PyCFunction)OSQP_get_workspace, METH_VARARGS, PyDoc_STR("Returns the OSQP workspace struct as a Python dictionary.")},
    {NULL, NULL}		/* sentinel */
};

static PyObject * OSQP_get_x(OSQP *self, void* context) {
    if (!self->workspace) {
        PyErr_SetString(PyExc_ValueError, "Workspace not initialized!");
        return NULL;
    }
    npy_intp n = (npy_intp)self->workspace->data->n;
    PyObject *x = (PyObject *)PyArrayFromCArray(self->workspace->x, &n);
    return x;
}

static PyObject * OSQP_get_y(OSQP *self, void* context) {
    if (!self->workspace) {
        PyErr_SetString(PyExc_ValueError, "Workspace not initialized!");
        return NULL;
    }
    npy_intp m = (npy_intp)self->workspace->data->m;
    PyObject *y = (PyObject *)PyArrayFromCArray(self->workspace->y, &m);
    return y;
}

static PyObject * OSQP_get_z(OSQP *self, void* context) {
    if (!self->workspace) {
        PyErr_SetString(PyExc_ValueError, "Workspace not initialized!");
        return NULL;
    }
    npy_intp m = (npy_intp)self->workspace->data->m;
    PyObject *z = (PyObject *)PyArrayFromCArray(self->workspace->z, &m);
    return z;
}

static PyObject * OSQP_get_z_tilde(OSQP *self, void* context) {
    if (!self->workspace) {
        PyErr_SetString(PyExc_ValueError, "Workspace not initialized!");
        return NULL;
    }
    c_int n = self->workspace->data->n;
    npy_intp m = (npy_intp)self->workspace->data->m;
    PyObject *z = (PyObject *)PyArrayFromCArray(self->workspace->xz_tilde + n, &m);
    return z;
}

static PyObject * OSQP_get_Ax(OSQP *self, void* context) {
    if (!self->workspace) {
        PyErr_SetString(PyExc_ValueError, "Workspace not initialized!");
        return NULL;
    }
    npy_intp m = (npy_intp)self->workspace->data->m;
    PyObject *z = (PyObject *)PyArrayFromCArray(self->workspace->Ax, &m);
    return z;
}

static PyObject * OSQP_get_lower_bound(OSQP *self, void* context) {
    if (!self->workspace) {
        PyErr_SetString(PyExc_ValueError, "Workspace not initialized!");
        return NULL;
    }
    npy_intp m = (npy_intp)self->workspace->data->m;
    PyObject *l = (PyObject *)PyArrayFromCArray(self->workspace->data->l, &m);
    return l;
}

static PyObject * OSQP_get_upper_bound(OSQP *self, void* context) {
    if (!self->workspace) {
        PyErr_SetString(PyExc_ValueError, "Workspace not initialized!");
        return NULL;
    }
    npy_intp m = (npy_intp)self->workspace->data->m;
    PyObject *u = (PyObject *)PyArrayFromCArray(self->workspace->data->u, &m);
    return u;
}

static PyObject * OSQP_get_rho_vec(OSQP *self, void* context) {
    if (!self->workspace) {
        PyErr_SetString(PyExc_ValueError, "Workspace not initialized!");
        return NULL;
    }
    npy_intp m = (npy_intp)self->workspace->data->m;
    PyObject *rho_vec = (PyObject *)PyArrayFromCArray(self->workspace->rho_vec, &m);
    return rho_vec;
}

static long pyarray_vector_size(PyArrayObject* obj) {
    int ndim = PyArray_NDIM(obj);
    if (ndim == 1)
        return PyArray_DIM(obj, 0);
    if (ndim != 2)
        return -1;
    if (PyArray_DIM(obj, 0) == 1)
        return PyArray_DIM(obj, 1);
    if (PyArray_DIM(obj, 1) == 1)
        return PyArray_DIM(obj, 0);
    return -1;
}

static int OSQP_set_rho_vec(OSQP *self, PyObject *value, void* context) {
    if (!self->workspace) {
        PyErr_SetString(PyExc_ValueError, "Workspace not initialized!");
        return -1;
    }

    if (!PyArray_Check(value)) {
        PyErr_SetString(PyExc_ValueError, "rho vec must be a numpy array");
        return -1;
    }

    npy_intp m = (npy_intp)self->workspace->data->m;
    int float_type = get_float_type();
    PyArrayObject *v_cont = get_contiguous((PyArrayObject*)value, float_type);
    int result;
    if (pyarray_vector_size(v_cont) != m) {
        PyErr_SetString(PyExc_ValueError, "rho vec size mismatch");
        result = -1;
    } else {
        c_float *v_arr = (c_float*)PyArray_DATA(v_cont);
        osqp_update_rho_vec(self->workspace, v_arr);
        result = 0;
    }
    
    Py_DECREF(v_cont);
    return result;
}

static PyGetSetDef RLQP_getset[] = {
    { "lower_bound", (getter)OSQP_get_lower_bound, NULL, PyDoc_STR("Get the lower bound"), NULL },
    { "upper_bound", (getter)OSQP_get_upper_bound, NULL, PyDoc_STR("Get the upper bound"), NULL },
    { "x", (getter)OSQP_get_x, NULL, PyDoc_STR("Get the current value of x"), NULL },
    { "y", (getter)OSQP_get_y, NULL, PyDoc_STR("Get the current value of y"), NULL },
    { "z", (getter)OSQP_get_z, NULL, PyDoc_STR("Get the current value of z"), NULL },
    { "z_tilde", (getter)OSQP_get_z_tilde, NULL, PyDoc_STR("Get the current value of z_tilde"), NULL },
    { "Ax", (getter)OSQP_get_Ax, NULL, PyDoc_STR("Get the current value of Ax"), NULL },
    { "rho_vec", (getter)OSQP_get_rho_vec, (setter)OSQP_set_rho_vec, PyDoc_STR("Access to the internal rho vector"), NULL },
    { NULL } /* sentinel */
};


// Define workspace type object
static PyTypeObject OSQP_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "rlqp.RLQP",                        /*tp_name*/
    sizeof(OSQP),                       /*tp_basicsize*/
    0,                                  /*tp_itemsize*/
    (destructor)OSQP_dealloc,           /*tp_dealloc*/
    0,                                  /*tp_print*/
    0,                                  /*tp_getattr*/
    0,                                  /*tp_setattr*/
    0,                                  /*tp_compare*/
    0,                                  /*tp_repr*/
    0,                                  /*tp_as_number*/
    0,                                  /*tp_as_sequence*/
    0,                                  /*tp_as_mapping*/
    0,                                  /*tp_hash */
    0,                                  /*tp_call*/
    0,                                  /*tp_str*/
    0,                                  /*tp_getattro*/
    0,                                  /*tp_setattro*/
    0,                                  /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,                 /*tp_flags*/
    "OSQP solver",                      /* tp_doc */
    0,		                            /* tp_traverse */
    0,		                            /* tp_clear */
    0,		                            /* tp_richcompare */
    0,		                            /* tp_weaklistoffset */
    0,		                            /* tp_iter */
    0,		                            /* tp_iternext */
    OSQP_methods,                       /* tp_methods */
    0,                                  /* tp_members */
    RLQP_getset,                        /* tp_getset */
    0,                                  /* tp_base */
    0,                                  /* tp_dict */
    0,                                  /* tp_descr_get */
    0,                                  /* tp_descr_set */
    0,                                  /* tp_dictoffset */
    (initproc)OSQP_init,                /* tp_init */
    0,                                  /* tp_alloc */
    0,                                  /* tp_new */
};

#endif
