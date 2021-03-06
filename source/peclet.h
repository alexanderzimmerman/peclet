#ifndef peclet_h
#define peclet_h

#include <deal.II/base/utilities.h>
#include <deal.II/base/quadrature_lib.h>
#include <deal.II/base/function.h>
#include <deal.II/base/logstream.h>
#include <deal.II/lac/vector.h>
#include <deal.II/lac/full_matrix.h>
#include <deal.II/lac/dynamic_sparsity_pattern.h>
#include <deal.II/lac/sparse_matrix.h>
#include <deal.II/lac/solver_cg.h>
#include <deal.II/lac/solver_bicgstab.h>
#include <deal.II/lac/precondition.h>
#include <deal.II/lac/constraint_matrix.h>
#include <deal.II/grid/tria.h>
#include <deal.II/grid/grid_generator.h>
#include <deal.II/grid/grid_refinement.h>
#include <deal.II/grid/tria_accessor.h>
#include <deal.II/grid/tria_iterator.h>
#include <deal.II/dofs/dof_handler.h>
#include <deal.II/dofs/dof_accessor.h>
#include <deal.II/dofs/dof_tools.h>
#include <deal.II/fe/fe_q.h>
#include <deal.II/fe/fe_values.h>
#include <deal.II/numerics/vector_tools.h>
#include <deal.II/numerics/matrix_tools.h>
#include <deal.II/numerics/solution_transfer.h>
#include <deal.II/numerics/error_estimator.h>
#include <deal.II/base/table_handler.h>

#include <iostream>
#include <functional>

#include <assert.h> 
#include <deal.II/grid/manifold_lib.h>
#include <deal.II/grid/tria_boundary_lib.h>
#include <deal.II/base/parsed_function.h>

#include "extrapolated_field.h"
#include "my_grid_generator.h"
#include "fe_field_tools.h"
#include "output.h"
#include "my_matrix_creator.h"
#include "my_vector_tools.h"

#include "peclet_parameters.h"

/*!

A namespace for everything specific to Peclet. This mostly just includes the Peclet class, but also
some related data.

@ingroup peclet

*/
namespace Peclet
{
    using namespace dealii;

    const double EPSILON = 1.e-14;

    struct SolverStatus
    {
        unsigned int last_step;
    };
  
    /*! This class solves the unsteady scalar convection-diffusion initial boundary value problem.

    The strong form of the initial boundary value problem (IBVP) is

    \f[

        u_t(\bf{x},t) + \bf{v}(x)\cdot\nabla u(x,t) - \nabla \cdot (\alpha(\bf{x})\nabla u(\bf{x},t)) = s(\bf{x},t) \forall \bf{x}, t \in \bf{\Omega} \times (t_0,t_f) \\
        
        u(\bf{x},0) = u_0(\bf{x}) \quad \forall \bf{x} \in \bf{\Omega} \\
        
        u(\bf{x},t) = g(\bf{x},t) \quad \forall \bf{x},t \in \bf{\Gamma}_D \times (t_0,t_f) \\
        
        \alpha(\bf{x})(\hat{\bf{n}}\cdot\nabla)u(\bf{x},t) = h(\bf{x},t) \quad \forall \bf{x},t \in \bf{\Gamma}_N \times (t_0,t_f)

    \f]

    Spatial derivatives are discretized with the standard Galerkin finite element method, and the temporal derivative is discretized with the $\theta$-family of semi-implicit finite difference methods.

    Linear system assembly and time stepping are based on deal.II Tutorial 26 by Wolfgang Bangerth, Texas A&M University, 2013

    Some of the more notable extensions (beyond step-26) include:
    - Builds convection-diffusion matrix instead of Laplace matrix
    - Supports time-dependent non-zero Dirichlet and Neumann boundary condition
    - Re-designed parmameter handling
    - Generalized boundary condition handling via the parameter input file
    - Writes FEFieldFunction to disk, and can read it from disk to initialize a restart
    - Extended the FEFieldFunction class for extrapolation
    - Added verification via Method of Manufactured Solutions (MMS) with error table based on approach from Tutorial 7
    - Added test suite using ctest and the standard deal.II approach
    - Added a parameteric sphere-cylinder grid
    - Added a boundary grid refinement routine
    - Added a output option for 1D solutions in tabular format

    A simulation can be run for example with the following main program:
    @code

        #include "peclet.h"

        int main(int argc, char* argv[])
        {   
            Peclet::Peclet<2> peclet;
            
            peclet.run();
            
            return 0;
        }

    @endcode

    The previous example uses default parameters and writes used_parameters.prm. A user can edit this file and save it as an input parameter file. The main program peclet/source/main.cc accepts an input parameter file path from the command line.

    Some methods of this class are decomposed into header files named peclet_*.h.
    All prototypes are implemented in header files to facilitate templating.

    @ingroup peclet
    */
    template<int dim>
    class Peclet
    {
    public:
      
        Peclet();
        
        /*! Structures all input parameters so that ParameterHandler can be discarded.
        
        This is public so that the parameters can be edited directly before calling Peclet:run().
            
        */
        Parameters::StructuredParameters params;
        
        /*! Run the simulation.
  
        This is the main method of the class.
      
        */
        void run(const std::string parameter_file = "");

    private:
    
        // Data members
        
        /*! The finite element triangulation, often just called tria in deal.II codes*/
        Triangulation<dim>   triangulation;
        
        /*! The Q1 finite element */
        FE_Q<dim>            fe;
        
        /*! The degrees of freedom handler
        
        This relates triangulation and fe to the system matrix.
            
        */
        DoFHandler<dim>      dof_handler;

        /*! The constraints matrix 
        
        This serves two purposes:
            1. Enforce Dirichlet boundary conditions.
            2. Apply hanging node constraints originating from local grid refinement.
        
        */
        ConstraintMatrix     constraints;
        
        /*! The sparsity pattern
        
        This maps values to the sparse system matrix.
        
        */
        SparsityPattern      sparsity_pattern;
        
        /*! The mass matrix, $M$
        
        This is the well known mass matrix arising from discretizing time via finite differences.
        
        */
        SparseMatrix<double> mass_matrix;
        
        /*! The convection-diffusion matrix, (C + K)
        
        This is the convection diffusion matrix, which is the sum of the convection matrix, C, and the well known stiffness (a.k.a. Laplace) matrix, K.
        
        Rather than assembling C and K separately and then summing them, this class assembles the convection-diffusion matrix element-wise with a single kernel, which is only a slight modfication of the Laplace matrix assembly routine.
        
        */
        SparseMatrix<double> convection_diffusion_matrix;
        
        /*! The system matrix
        
        This is the composite matrix for the entire linear system.
            
        */
        SparseMatrix<double> system_matrix;

        /*! The solution vector */
        Vector<double>       solution;
        
        /*! The solution vector from the previous time step */
        Vector<double>       old_solution;
        
        /*! The composite right-hand side of the entire linear system */
        Vector<double>       system_rhs;

        /*! The current time for the time-dependent simulation */
        double               time;
        
        /*! The time step size for the time-dependent simulation 
        
        Note that this is constant for any call to Peclet::run().
        
        */
        double               time_step_size;
        
        /*! A counter to track the current time step index */
        unsigned int         time_step_counter;
        
        /*! Geometric information required for exact spherical geometry */
        Point<dim> spherical_manifold_center;
        
        /*! These ID's label manifolds used for exact geometry */
        std::vector<unsigned int> manifold_ids;
        
        /*! These strings label types of manifolds used for exact geometry */
        std::vector<std::string> manifold_descriptors;
        
        // Function data members
        
        /*! A pointer to a deal.II Function for spatially variable convection velocity */
        Function<dim>* velocity_function;
        
        /*! A pointer to a deal.II Function for spatially variable thermal diffusivity */
        Function<dim>* diffusivity_function;
        
        /*! A pointer to a deal.II Function for a spatially and temporally variable source */
        Function<dim>* source_function;
        
        /*! A vector of pointers to deal.II Functions for spatially and temporally variable boundary conditions */
        std::vector<Function<dim>*> boundary_functions;
        
        /*! A pointer to a deal.II Function for spatially variable initial values */
        Function<dim>* initial_values_function;
        
        /*! A pointer to a deal.II Function for spatially and temporally variable exact solution
            
        Of course this is only used for code verification purposes, when an exact solution is known. See the tests involving the method of manufactured solution (MMS).
            
        */
        Function<dim>* exact_solution_function;
        
        /*! A deal.II TableHandler for tabulating convergence/verification data */
        TableHandler verification_table;
        
        /*! The path where to write the table containing convergence/verification data 
        
        Also see Peclet::verification_table.
        
        */
        std::string verification_table_file_name = "verification_table.txt";
        
        /*! A deal.II TableHandler for tabulating 1D solution data
    
        This has primarily been used as a convenient output for importing the 1D data into MATLAB.
        
        This is impractical for large problems, which should use the standard visualization formats for tools such as ParaView or VisIt.
    
        */        
        TableHandler solution_table_1D;
        
        /*! The path where to write the table containing 1D solution data 
        
        Also see Peclet::solution_table_1D.
        
        */
        std::string solution_table_1D_file_name = "1D_solution_table.txt";
        
        
        // Methods
        
        /*! Solve a time step. 

        This involves solving the linear system based on the homogeneous part of the solution, recovering the inhomogeneous solution via the constraints matrix, and applying hanging node constraints also via the constraints matrix.

        */
        SolverStatus solve_time_step(bool quiet = false);
        
        /*! Set the coarse Triangulation, i.e. Peclet::triangulation.

        In deal.II language, coarse means the coarsest available representation of the geometry. In combination with geometric manifolds, this can indeed be quite coarse, even for curved geometries. For example, see the hemisphere_cylinder_shell in MyGridGenerator.
        
        Unfortunately geometric manifold data is not contained by the Triangulation. This separation allows for flexibility, but also requires extra bookkeeping by the programmer.

        Peclet<1>::create_coarse_grid(), Peclet<2>::create_coarse_grid(), and Peclet<3>::create_coarse_grid() had to be implemented separately because GridTools::rotate() has not be implemented for Triangulation<1>.
            
        */
        void create_coarse_grid();
      
        /*! Adaptively refine the triangulation based on an error measure.
            
        This is mostly a copy of the routine from deal.II's step-26, which uses the Kelly Error Estimator.
                
        */
        void adaptive_refine();
        
        /*! Re-initialize the linear system data and assemble the important matrices.
        
        This involves a few very important steps:
            - initializing hanging node constraints, the sparsity pattern, all matrices
            - assemble the mass and convection-diffusion matrices
            - reinitialize solution vectors
            
        The separation between this method and some of the work done in Peclet::run() may not be very well organized. There may be a better approach. That being said, little has been changed here relative to the step-26 tutorial.
            
        */
        void setup_system(bool quiet = false);
        
        /*! Write the solution to files for visualization.
  
        Only the VTK format is supported by this class; but deal.II makes it easy to use many other standard formats.
        
        Additionally for 1D problems, a simple table can be written for easy import into MATLAB.
      
        */
        void write_solution();
        
        /*! Append convergence/verification data to the table in memory.
    
        This calculates both L2 and L1 norms based on a provided exact solution.
        
        */
        void append_verification_table();
        
        /*! Write convergence/verification data to disk. */
        void write_verification_table();
        
        /*! Append 1D solution data to the table in memory. */
        void append_1D_solution_to_table();
        
        /*! Write 1D solution data to disk. */
        void write_1D_solution_table(std::string file_name);
    };
  
    template<int dim>
    Peclet<dim>::Peclet()
        :
        fe(1),
        dof_handler(this->triangulation)
    {}
  
    #include "peclet_grid.h"
  
    template<int dim>
    void Peclet<dim>::setup_system(bool quiet)
    {
        dof_handler.distribute_dofs(fe);

        if (!quiet)
        {
            std::cout << std::endl
                  << "==========================================="
                  << std::endl
                  << "Number of active cells: " << triangulation.n_active_cells()
                  << std::endl
                  << "Number of degrees of freedom: " << dof_handler.n_dofs()
                  << std::endl
                  << std::endl;    
        }

        constraints.clear();
        
        DoFTools::make_hanging_node_constraints(
            dof_handler,
            constraints);
            
        constraints.close();

        DynamicSparsityPattern dsp(dof_handler.n_dofs());
        
        DoFTools::make_sparsity_pattern(
            this->dof_handler,
            dsp,
            this->constraints,
            /*keep_constrained_dofs = */ true);
            
        this->sparsity_pattern.copy_from(dsp);

        this->mass_matrix.reinit(this->sparsity_pattern);
        
        this->convection_diffusion_matrix.reinit(this->sparsity_pattern);
        
        this->system_matrix.reinit(this->sparsity_pattern);

        MatrixCreator::create_mass_matrix(
            this->dof_handler,
            QGauss<dim>(fe.degree+1),
            this->mass_matrix);
                              
        MyMatrixCreator::create_convection_diffusion_matrix<dim>(
            this->dof_handler,
            QGauss<dim>(fe.degree+1),
            this->convection_diffusion_matrix,
            this->diffusivity_function, 
            this->velocity_function);

        this->solution.reinit(dof_handler.n_dofs());
        
        this->old_solution.reinit(dof_handler.n_dofs());
        
        this->system_rhs.reinit(dof_handler.n_dofs());
        
    }

    template<int dim>
    SolverStatus Peclet<dim>::solve_time_step(bool quiet)
    {
        double tolerance = this->params.solver.tolerance;
        
        if (this->params.solver.normalize_tolerance)
        {
            tolerance *= this->system_rhs.l2_norm();
        }
        
        SolverControl solver_control(
            this->params.solver.max_iterations,
            tolerance);
           
        SolverCG<> solver_cg(solver_control);
        
        SolverBicgstab<> solver_bicgstab(solver_control);

        PreconditionSSOR<> preconditioner;
        
        preconditioner.initialize(this->system_matrix, 1.0);

        std::string solver_name;
        
        if (this->params.solver.method == "CG")
        {
            solver_name = "CG";
        
            solver_cg.solve(
                this->system_matrix,
                this->solution,
                this->system_rhs,
                preconditioner);    
        }
        else if (this->params.solver.method == "BiCGStab")
        {
            solver_name = "BiCGStab";
            
            solver_bicgstab.solve(
                this->system_matrix,
                this->solution,
                this->system_rhs,
                preconditioner);
        }

        this->constraints.distribute(this->solution);

        if (!quiet)
        {
            std::cout << "     " << solver_control.last_step()
                  << " " << solver_name << " iterations." << std::endl;
        }
        
        SolverStatus status;
        
        status.last_step = solver_control.last_step();
        
        return status;
        
    }
  
    #include "peclet_1D_solution_table.h"
  
    template<int dim>
    void Peclet<dim>::write_solution()
    {
          
        if (this->params.output.write_solution_vtk)
        {
            Output::write_solution_to_vtk(
                "solution-"+Utilities::int_to_string(this->time_step_counter)+".vtk",
                this->dof_handler,
                this->solution);    
        }
        
        if (dim == 1)
        {
            this->append_1D_solution_to_table();
        }
        
    }
  
    template<int dim>
    void Peclet<dim>::append_verification_table()
    {
        assert(this->params.verification.enabled);
        
        this->exact_solution_function->set_time(this->time);
        
        Vector<float> difference_per_cell(triangulation.n_active_cells());
        
        VectorTools::integrate_difference(
            this->dof_handler,
            this->solution,
            *this->exact_solution_function,
            difference_per_cell,
            QGauss<dim>(3),
            VectorTools::L2_norm);
            
        double L2_norm_error = difference_per_cell.l2_norm();
        
        VectorTools::integrate_difference(
            this->dof_handler,
            this->solution,
            *this->exact_solution_function,
            difference_per_cell,
            QGauss<dim>(3),
            VectorTools::L1_norm);
            
        double L1_norm_error = difference_per_cell.l1_norm();
            
        this->verification_table.add_value("time_step_size", this->time_step_size);
        
        this->verification_table.add_value("time", this->time);
        
        this->verification_table.add_value("cells", this->triangulation.n_active_cells());
        
        this->verification_table.add_value("dofs", this->dof_handler.n_dofs());
        
        this->verification_table.add_value("L1_norm_error", L1_norm_error);
        
        this->verification_table.add_value("L2_norm_error", L2_norm_error);
        
    }
  
    template<int dim>
    void Peclet<dim>::write_verification_table()
    {
        const int precision = 14;
        
        this->verification_table.set_precision("time", precision);
        
        this->verification_table.set_scientific("time", true);
        
        this->verification_table.set_precision("time_step_size", precision);
        
        this->verification_table.set_scientific("time_step_size", true);
        
        this->verification_table.set_precision("cells", precision);
        
        this->verification_table.set_scientific("cells", true);
        
        this->verification_table.set_precision("dofs", precision);
        
        this->verification_table.set_scientific("dofs", true);
        
        this->verification_table.set_precision("L2_norm_error", precision);
        
        this->verification_table.set_scientific("L2_norm_error", true);
        
        this->verification_table.set_precision("L1_norm_error", precision);
        
        this->verification_table.set_scientific("L1_norm_error", true);
        
        std::ofstream out_file(this->verification_table_file_name, std::fstream::app);
        
        assert(out_file.good());
        
        this->verification_table.write_text(out_file);
        
        out_file.close(); 

    }

  template<int dim>
  void Peclet<dim>::run(const std::string parameter_file)
  {
    
    /* Clean up the files in the working directory */
    
    if (dim == 1)
    {
        std::remove(solution_table_1D_file_name.c_str()); // In 1D, the solution will be appended here at every time step.    
    }        
    
    if (this->params.verification.enabled)
    {
        std::remove(this->verification_table_file_name.c_str());
    }
    
    /*
    Working with deal.II's Function class has been interesting, and I'm 
    sure many of my choices are unorthodox. The most important lesson learned has been that 
    a Function<dim>* can point to any class derived from Function<dim>. This is generally true
    for derived classes in C++. In this code, the general design pattern then is to 
    instantitate all of the functions that might be needed, and then to point to the ones
    actually being used. The extra instantiations don't cost us anything.
    */
    Functions::ParsedFunction<dim> parsed_velocity_function(dim),
        parsed_diffusivity_function,
        parsed_source_function,
        parsed_boundary_function,
        parsed_initial_values_function,
        parsed_exact_solution_function; 
    
    this->params = Parameters::read<dim>(
        parameter_file,
        parsed_velocity_function,
        parsed_diffusivity_function,
        parsed_source_function,
        parsed_boundary_function,
        parsed_exact_solution_function,
        parsed_initial_values_function);
    
    this->create_coarse_grid();
    
    this->velocity_function = &parsed_velocity_function;
    
    this->diffusivity_function = &parsed_diffusivity_function;
    
    this->source_function = &parsed_source_function;
    
    this->exact_solution_function = &parsed_exact_solution_function;
    
    /*
    Generalizing the handling of auxiliary functions is complicated. In most cases one should be able to use a ParsedFunction, but the generality of Function<dim>* allows for a standard way to account for any possible derived class of Function<dim>. 
    For example this allows for....
        - an optional initial values function that interpolates an old solution loaded from disk. 
        - flexibily implementing general boundary conditions
    */

    /* Initial values function */
    Triangulation<dim> field_grid;
    
    DoFHandler<dim> field_dof_handler(field_grid);
    
    Vector<double> field_solution;
    
    if (this->params.initial_values.function_name != "interpolate_old_field")
    { // This will write files that need to exist.
        this->setup_system(true);
        FEFieldTools::save_field_parts(this->triangulation, this->dof_handler, this->solution); 
    }
    
    FEFieldTools::load_field_parts(
        field_grid,
        field_dof_handler,
        field_solution,
        this->fe);
    
    MyFunctions::ExtrapolatedField<dim> field_function(
        field_dof_handler,
        field_solution);
    

    if (this->params.initial_values.function_name == "interpolate_old_field")
    {
        
        this->initial_values_function = &field_function;
        
    }
    else if (this->params.initial_values.function_name == "parsed")
    {
        
        this->initial_values_function = &parsed_initial_values_function;
        
    }
    
    /* Boundary condition functions */
    
    unsigned int boundary_count = this->params.boundary_conditions.implementation_types.size();
    
    assert(params.boundary_conditions.function_names.size() == boundary_count);

    std::vector<ConstantFunction<dim>> constant_functions;
    
    for (unsigned int boundary = 0; boundary < boundary_count; boundary++)
    {
        std::string boundary_type = this->params.boundary_conditions.implementation_types[boundary];
        
        std::string function_name = this->params.boundary_conditions.function_names[boundary];
        
        if (function_name == "constant")
        {
            
            double value = this->params.boundary_conditions.function_double_arguments.front();
        
            this->params.boundary_conditions.function_double_arguments.pop_front();
            
            constant_functions.push_back(ConstantFunction<dim>(value));
            
        }
        
    }
        
    /* Organize boundary functions to simplify application during the time loop */
    
    unsigned int constant_function_index = 0;
    
    for (unsigned int boundary = 0; boundary < boundary_count; boundary++)        
    {
        std::string boundary_type = this->params.boundary_conditions.implementation_types[boundary];
        
        std::string function_name = this->params.boundary_conditions.function_names[boundary];

        if (function_name == "constant")
        {
            
            assert(constant_function_index < constant_functions.size());
        
            this->boundary_functions.push_back(&constant_functions[constant_function_index]);
            
            constant_function_index++;
            
        }
        else if (function_name == "parsed")
        {
            
            this->boundary_functions.push_back(&parsed_boundary_function);
            
        }
        
    }
    
    /* Attach manifolds for exact geometry */
    
    assert(dim < 3); 
    /* @todo 3D extension: For now the CylindricalManifold is being ommitted.
    
    deal.II makes is impractical for a CylindricalManifold to exist in 2D.
        
    */
    
    SphericalManifold<dim> spherical_manifold(this->spherical_manifold_center);
    
    for (unsigned int i = 0; i < manifold_ids.size(); i++)
    {
        if (manifold_descriptors[i] == "spherical")
        {
            this->triangulation.set_manifold(manifold_ids[i], spherical_manifold);      
        }
    }
    
    /* Run initial grid refinement cycles */
    this->triangulation.refine_global(this->params.refinement.initial_global_cycles);
    
    Refinement::refine_mesh_near_boundaries(
        this->triangulation,
        this->params.refinement.boundaries_to_refine,
        this->params.refinement.initial_boundary_cycles);
        
    /* Initialize the linear system and constraints */
    this->setup_system(); 

    Vector<double> tmp;
    
    Vector<double> forcing_terms;
    
    double epsilon = 1e-14;
    
    /* Iterate through time steps
    
    A goto statement (to the start_time_iteration label) is used to handle pre-refinement.
    Generally goto's are a terrible idea; but the step-26 tutorial makes a case for it being instructive here.
    It would probably be better to redesign this without a goto.
    
    */
    unsigned int pre_refinement_step = 0;
    
start_time_iteration: 

    tmp.reinit(this->solution.size());

    VectorTools::interpolate(this->dof_handler,
                             *this->initial_values_function,
                             this->old_solution); 
    
    this->solution = this->old_solution;
    
    this->write_solution(); /* Write the initial values */
    
    this->time_step_counter = 0;
    
    this->time = 0;
    
    double theta = this->params.time.semi_implicit_theta;
    
    this->time_step_size = this->params.time.step_size;
    
    if (this->time_step_size < EPSILON)
    {
        this->time_step_size = this->params.time.end_time/
            pow(2., this->params.time.global_refinement_levels);
    }
    
    double Delta_t = this->time_step_size;
    
    bool final_time_step = false;
    
    bool output_this_step = true;
    
    SolverStatus solver_status;
    
    do
    {
        ++this->time_step_counter;
        
        /* Typically you see something more like "time += Delta_t" in time-dependent codes,
            but that method accumulates finite-precision roundoff errors. This is a better way. */
        time = Delta_t*time_step_counter;
        
        /* Set some flags that will control output for this step. */
        final_time_step = this->time > this->params.time.end_time - epsilon;
        
        bool at_interval = false;
        
        if (this->params.output.time_step_interval == 1)
        {
            at_interval = true;
        }
        else if (this->params.output.time_step_interval != 0)
        {
            if ((time_step_counter % this->params.output.time_step_interval) == 0)
            {
                at_interval = true;
            }
        }
        else
        {
            at_interval = false;
        }
        
        if (at_interval)
        {
            output_this_step = true;
        }
        else
        {
            output_this_step = false;
        }
        
        /* Report the time step index and time. */
        if (output_this_step)
        {
            std::cout << "Time step " << this->time_step_counter 
                << " at t=" << this->time << std::endl;    
        }

        /* Add mass and convection-diffusion matrix terms to the RHS. */
        this->mass_matrix.vmult(this->system_rhs, this->old_solution);

        this->convection_diffusion_matrix.vmult(tmp, this->old_solution);
        
        this->system_rhs.add(-(1. - theta)*Delta_t, tmp);
        
        /* Add source/forcing terms to the RHS. */
        source_function->set_time(this->time);
        
        VectorTools::create_right_hand_side(
            this->dof_handler,
            QGauss<dim>(fe.degree+1),
            *source_function,
            tmp);
        
        forcing_terms = tmp;
        
        forcing_terms *= Delta_t*theta;
        
        source_function->set_time(this->time - Delta_t);
        
        VectorTools::create_right_hand_side(
            this->dof_handler,
            QGauss<dim>(fe.degree + 1),
            *source_function,
            tmp);
        
        forcing_terms.add(Delta_t*(1 - theta), tmp);
        
        this->system_rhs += forcing_terms;
        
        /* Add natural boundary conditions to RHS */
        for (unsigned int boundary = 0; boundary < boundary_count; boundary++)
        {
            if ((this->params.boundary_conditions.implementation_types[boundary] != "natural"))
            {
                continue;
            }
            
            std::set<types::boundary_id> dealii_boundary_id = {boundary}; /* @todo: This throws a warning */
            
            boundary_functions[boundary]->set_time(this->time);
            
            MyVectorTools::my_create_boundary_right_hand_side(
                this->dof_handler,
                QGauss<dim-1>(fe.degree + 1),
                *boundary_functions[boundary],
                tmp,
                dealii_boundary_id);
                
            forcing_terms = tmp;
            
            forcing_terms *= Delta_t*theta;
                
            boundary_functions[boundary]->set_time(this->time - Delta_t);
            
            MyVectorTools::my_create_boundary_right_hand_side(
                this->dof_handler,
                QGauss<dim-1>(fe.degree + 1),
                *boundary_functions[boundary],
                tmp,
                dealii_boundary_id);
             
            forcing_terms.add(Delta_t*(1. - theta), tmp);
            
            this->system_rhs += forcing_terms;

        }
        
        /* Make the system matrix and apply constraints. */
        system_matrix.copy_from(mass_matrix);
        
        system_matrix.add(theta*Delta_t, convection_diffusion_matrix);

        constraints.condense(system_matrix, system_rhs);

        {
            /* Apply strong boundary conditions */
            std::map<types::global_dof_index, double> boundary_values;
            
            for (unsigned int boundary = 0; boundary < boundary_count; boundary++)
            {
                if (this->params.boundary_conditions.implementation_types[boundary] != "strong") 
                {
                    continue;
                }
                
                boundary_functions[boundary]->set_time(time);
                
                VectorTools::interpolate_boundary_values(
                    this->dof_handler,
                    boundary,
                    *boundary_functions[boundary],
                    boundary_values
                    );
                    
            }
            
            MatrixTools::apply_boundary_values(
                boundary_values,
                this->system_matrix,
                this->solution,
                this->system_rhs);
                
        }

        solver_status = this->solve_time_step(!output_this_step);
        
        /* Check if a steady state has been reached. */
        if ((this->params.time.stop_when_steady) & (solver_status.last_step == 0))
        {
            std::cout << "Reached steady state at t = " << this->time << std::endl;
            
            final_time_step = true;
            
            output_this_step = true;
            
        }
        
        /* Write the solution. */
        if (output_this_step)
        {
            this->write_solution();
            
            if (this->params.verification.enabled)
            {
                this->append_verification_table();
            }
            
        }
        
        /* Adaptively refine the grid. */
        if ((time_step_counter == 1) &&
            (pre_refinement_step < this->params.refinement.adaptive.initial_cycles))
        {
            this->adaptive_refine();
            
            ++pre_refinement_step;

            tmp.reinit(this->solution.size());

            std::cout << std::endl;

            goto start_time_iteration;
            
        }
        else if ((time_step_counter > 0) 
                 && (params.refinement.adaptive.interval > 0)  // % 0 (mod 0) is undefined
                 && (time_step_counter % params.refinement.adaptive.interval == 0))
        {
            
            for (unsigned int cycle = 0;
                 cycle < params.refinement.adaptive.cycles_at_interval; cycle++)
            {
                this->adaptive_refine();
            }
            
            tmp.reinit(this->solution.size());
            
        }
        
        this->old_solution = this->solution;
        
    } while (!final_time_step);
    
    /* Write FEFieldFunction related data so that it can be used as initial values for another run. */
    FEFieldTools::save_field_parts(triangulation, dof_handler, solution);
    
    /* Write the convergence/verification table. */
    if (this->params.verification.enabled)
    {
        this->write_verification_table();
    }
    
    /* Write the 1D solution table */
    if (dim == 1)
    {
        this->write_1D_solution_table(this->solution_table_1D_file_name);
    }
    
    /* Clean up. 
    
        Manifolds must be detached from Triangulations before leaving this scope.
    
    */
    this->triangulation.set_manifold(0);
    
    }
    
}

#endif