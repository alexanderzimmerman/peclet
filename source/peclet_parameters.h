#ifndef peclet_parameters_h
#define peclet_parameters_h

#include <vector>
#include <iostream>
#include <fstream>
#include <functional>

#include <deal.II/base/parameter_handler.h>

/*! Encapsulates parameter handling and paramter input file handling.
  
    Originally the ParameterReader from deal.II's step-26 was used;
    but that was too prohibitive. The approach in this file isolates the details
    of the input file handling from the rest of the program,
    allowing for rich data structures and simplifying the user code.

    The goal is to allow one to run the program with different parameters,
    without having to recomplile the program.

    It is valuable to structure the parameter data as done here, to greatly
    simplify the writing and debugging of the code.

    This also makes it simple to insantiate a PDE Model in a user program 
    and then change it's parameters directly without having to use any intermediate text files.
        
    @todo Allow for multiple parsed boundary functions.
        This might not be possible.
        This will obsolete the "constant" function option.
    
*/

namespace Peclet
{
    /*!
    
    Contains parameters data structures and methods for reading from the input file 
    
    @ingroup parameters
    */
    namespace Parameters
    {   

        using namespace dealii;

        /*! Contain parameters that are required for instantiating a Peclet::Peclet */
        struct Meta
        {
            unsigned int dim;
        };

        /*! Contains parameters for boundary conditions */
        struct BoundaryConditions
        {
            std::vector<std::string> implementation_types;
            std::vector<std::string> function_names;
            std::list<double> function_double_arguments;
        };
        
        /*! Contains parameters for initial values */
        struct InitialValues
        {
            std::string function_name;
            std::list<double> function_double_arguments; 
        };
        
        /*! Contains parameters for geometry */
        struct Geometry
        {
            unsigned int dim;
            std::string grid_name;
            std::vector<double> sizes;
            std::vector<double> transformations;
        };
        
        /*! Contains parameters for adaptive grid refinement 
            
            Also see Peclet::Parameters::Refinement
            
        */
        struct AdaptiveRefinement
        {
            unsigned int initial_cycles;
            unsigned int max_level;
            unsigned int max_cells;
            unsigned int interval;
            unsigned int cycles_at_interval;
            double refine_fraction;
            double coarsen_fraction;
        };
        
        /*! Contains parameters for grid refinement */
        struct Refinement
        {
            unsigned int initial_global_cycles;
            unsigned int initial_boundary_cycles;
            std::vector<unsigned int> boundaries_to_refine;
            AdaptiveRefinement adaptive;
        };
        
        /*! Contains parameters for time integration */
        struct Time
        {
            double end_time;
            double step_size;
            double global_refinement_levels;
            double semi_implicit_theta;
            bool stop_when_steady;
        };
        
        /*! Contains parameters for the iterative solver */
        struct IterativeSolver
        {
            std::string method;
            unsigned int max_iterations;
            double tolerance;
            bool normalize_tolerance;
        };
        
        /*! Contains parameters for solution output to file */
        struct Output
        {
            bool write_solution_vtk;
            bool write_solution_table;
            int time_step_interval;
        };
        
        /*! Contains parameters for verification against an exact solution */
        struct Verification
        {
            bool enabled;
            std::string exact_solution_function_name;
            std::vector<double> exact_solution_function_double_arguments;
        };
        
        /*! Contains are parameter data structures */
        struct StructuredParameters
        {
            Meta meta;
            BoundaryConditions boundary_conditions;
            InitialValues initial_values;
            Geometry geometry;
            Refinement refinement;
            Time time;
            IterativeSolver solver;
            Output output;
            Verification verification;
        };    

        /*! Declare parmaeters using dealii::ParameterHandler */
        template<int dim>
        void declare(ParameterHandler &prm)
        {
            
            prm.enter_subsection("meta");
            {
                prm.declare_entry("dim", std::to_string(dim), Patterns::Integer(1, 3),
                    "The number of spatial dimensions, either 1, 2, or 3.");
            }
            prm.leave_subsection();
            
            
            prm.enter_subsection("parsed_velocity_function");
            {
                Functions::ParsedFunction<dim>::declare_parameters(prm, dim);    
            }
            prm.leave_subsection();

            
            prm.enter_subsection("parsed_diffusivity_function");
            {
                Functions::ParsedFunction<dim>::declare_parameters(prm);    
            }
            prm.leave_subsection();

            
            prm.enter_subsection("parsed_source_function");
            {
                Functions::ParsedFunction<dim>::declare_parameters(prm);    
            }
            prm.leave_subsection();  
            
            
            prm.enter_subsection ("geometry");
            {
                    
                prm.declare_entry("grid_name", "hyper_cube",
                     Patterns::Selection(
                        "hyper_rectangle | hyper_cube | hyper_shell | hemisphere_cylinder_shell"
                        " | cylinder | cylinder_with_split_boundaries"
                        " | hyper_cube_with_cylindrical_hole"),
                     
                     "Select the name of the geometry and grid to generate."
                     "\nMost of these geometries are implemented in the deal.II library,"
                     " so it is important to review the deal.II documentation for the geometry."
                     " For the user's convenience, here is some boundary ID information:"
                     
                     "\nhyper_rectangle"
                     "\n\tBoundary ID's in 2D:"
                     "\n\t0: x_min"
                     "\n\t1: x_max"
                     "\n\t2: y_min"
                     "\n\t3: y_max"
                     
                     "\nhyper_shell"
                     "\n\tInner boundary ID = 0"
                     "\n\tOuter boundary ID = 1"
                     
                     "\nhemisphere_cylinder_shell"
                     "\n\tBoundary ID's in 2D:"
                     "\n\t0: Right half of outer circle"
                     "\n\t1: Right side outer rectangle"
                     "\n\t2: Top of outer rectangle"
                     "\n\t3: Left side of outer rectangle"
                     "\n\t4: Left side of outer circle"
                     "\n\t5: Right half of inner circle"
                     "\n\t6: Right side inner rectangle"
                     "\n\t7: Top of inner rectangle"
                     "\n\t8: Left side of inner rectangle"
                     "\n\t9: Left side of inner circle"
                     
                     "\ncylinder"
                     "\n\t\t0: Heat flux"
                     "\n\t\t1: Outflow"
                     "\n\t\t2: Domain sides"
                     "\n\t\t3: Inflow"
                     
                     "\nhyper_cube_with_cylindrical_hole:"
                     "\n\tOuter boundary ID = 0"
                     "\n\tInner spherical boundary ID = 1");
                     
                prm.declare_entry("sizes", "0., 1.",
                    Patterns::List(Patterns::Double(0.)),
                    "Set the sizes for the grid's geometry."
                    "\n hyper_rectangle:"
                        "{Point0.x, Point0.y, Point1.x, Point1.y}"
                    "\n hyper_shell:"
                        "{inner_radius, outer_radius}"
                    "\n  hemisphere_cylinder_shell: "
                         "{inner_sphere_radius, outer_sphere_radius, "
                         "inner_cylinder_length, outer_cylinder_length}"
                    "\n cylinder: "
                        "{L0, L1, L2}"
                    "\n  hyper_cube_with_cylindrical_hole : {hole_radius, half_of_outer_edge_length}");
                    
                prm.declare_entry("transformations", "0., 0., 0.",
                    Patterns::List(Patterns::Double()),
                    "Set the rigid body transformation vector."
                    " This transformation will be applied to the coarse grid."
                    "\n  2D : {shift_along_x, shift_along_y, rotate_about_z}"
                    "\n  3D : {shift_along_x, shift_along_y, shift_along_z, "
                              "rotate_about_x, rotate_about_y, rotate_about_z}");
                              
            }
            prm.leave_subsection ();
            
            
            prm.enter_subsection ("initial_values");
            {
                prm.declare_entry(
                    "function_name",
                    "parsed",
                    Patterns::List(Patterns::Selection("parsed | constant | interpolate_old_field")),
                    "Choose to either use a parsed function for the initial values, "
                    "or to interpolate them from an existing FEFieldFunction, "
                    "e.g. from an old solution. ");
                    
                prm.declare_entry("function_double_arguments", "",
                    Patterns::List(Patterns::Double()),
                    "This is deprecated."); 
                    
                prm.enter_subsection("parsed_function");
                {
                    Functions::ParsedFunction<dim>::declare_parameters(prm); 
                }
                prm.leave_subsection();
                    
            }
            prm.leave_subsection ();
            
            
            prm.enter_subsection ("boundary_conditions");
            {
                // Each of these lists needs a value for every boundary, in order
                prm.declare_entry("implementation_types", "natural, strong",
                    Patterns::List(Patterns::Selection("natural | strong")),
                    "Specify the type of boundary conditions to apply to each boundary."
                    " A value is required for every boundary ID in the coarse grid.");  
                    
                prm.declare_entry("function_names", "parsed, parsed",
                    Patterns::List(Patterns::Selection("parsed | constant")),
                    "Specify the names of functions to apply to each boundary."
                    " A value is required for every boundary ID in the coarse grid.");
                    
                prm.declare_entry("function_double_arguments", "",
                    Patterns::List(Patterns::Double()),
                    "This is deprecated."); 
                    
                prm.enter_subsection("parsed_function");
                {
                    Functions::ParsedFunction<dim>::declare_parameters(prm);    
                }
                prm.leave_subsection();

            }
            prm.leave_subsection ();
            
            
            prm.enter_subsection ("refinement");
            {
                prm.declare_entry("initial_global_cycles", "4",
                    Patterns::Integer(),
                    "Initially globally refine the grid this many times "
                    "without using any error measure");
                    
                prm.declare_entry("initial_boundary_cycles", "0",
                    Patterns::Integer(),
                    "Initially refine the grid this many times"
                    "near the boundaries that are listed for refinement");
                    
                prm.declare_entry("boundaries_to_refine", "0",
                    Patterns::List(Patterns::Integer()),
                    "Refine cells that contain these boundaries");
                    
                prm.enter_subsection ("adaptive");
                {
                    prm.declare_entry("initial_cycles", "0",
                        Patterns::Integer(),
                        "Refine grid adaptively using an error measure "
                        "this many times before beginning the time stepping.");
                        
                    prm.declare_entry("interval", "0",
                        Patterns::Integer(),
                        "Only refine the grid after every occurence of "
                        "this many time steps.");
                        
                    prm.declare_entry("max_level", "10",
                        Patterns::Integer(),
                        "Max grid refinement level");
                        
                    prm.declare_entry("max_cells", "2000",
                        Patterns::Integer(),
                        "Skip grid refinement if the number of active cells "
                        "already exceeds this");
                        
                    prm.declare_entry("refine_fraction", "0.3",
                        Patterns::Double(),
                        "Fraction of cells to refine");
                        
                    prm.declare_entry("coarsen_fraction", "0.3",
                        Patterns::Double(),
                        "Fraction of cells to coarsen");
                        
                    prm.declare_entry("cycles_at_interval", "5",
                        Patterns::Integer(),
                        "Max grid refinement level");
                        
                }
                prm.leave_subsection();
                
            }
            prm.leave_subsection();
            
            
            prm.enter_subsection ("time");
            {
                prm.declare_entry("end_time", "1.",
                    Patterns::Double(0.),
                    "End the time-dependent simulation once this time is reached.");
                    
                prm.declare_entry("step_size", "0.",
                    Patterns::Double(0.),
                    "End the time-dependent simulation once this time is reached."
                    "\nSet to zero to instead use global_refinement_levels");
                    
                prm.declare_entry("global_refinement_levels", "4",
                    Patterns::Integer(0),
                    "If step_size is set to zero, then compute "
                    "step_size = end_time/(2^global_refinement_levels)");
                    
                prm.declare_entry("semi_implicit_theta", "0.5",
                    Patterns::Double(0., 1.),
                    "This is the theta parameter for the theta-family of "
                    "semi-implicit time integration schemes."
                    " Choose any value between zero and one."
                    " 0 = fully explicit; 0.5 = 'Crank-Nicholson'"
                    " ; 1 = fully implicit");
                    
                prm.declare_entry("stop_when_steady", "false",
                    Patterns::Bool(),
                    "If true, then stop when solver reports zero iterations"
                    " instead of waiting for end_time");
                    
            }
            prm.leave_subsection();
            
            prm.enter_subsection("solver");
            {
                prm.declare_entry("method", "CG",
                     Patterns::Selection("CG | BiCGStab"),
                     "Select an iterative method for solving the linear system.");
                     
                prm.declare_entry("max_iterations", "1000",
                    Patterns::Integer(0),
                    "Set the maxinum number of iterations for solving the linear system.");
                    
                prm.declare_entry("tolerance", "1e-8",
                    Patterns::Double(0.),
                    "Set the convergence tolerance for the iterative method.");
                    
                prm.declare_entry("normalize_tolerance", "false",
                    Patterns::Bool(),
                    "If true, then the residual will be multiplied by the L2-norm of the RHS"
                    " before comparing to the convergence tolerance.");
            }
            prm.leave_subsection();
            
            prm.enter_subsection("output");
            {
                prm.declare_entry("write_solution_vtk", "true", Patterns::Bool(),
                "Write the solution to VTK files for visualization in Paraview or VisIt.");
                
                prm.declare_entry("write_solution_table", "false", Patterns::Bool(),
                    "This allow for simple export of 1D solutions into a table format"
                    " easily read by MATLAB."
                    "\nThe way this is currently implemented takes a great deal of memory"
                    ", so you should probably only use this in 1D.");
                    
                prm.declare_entry("time_step_interval", "1", Patterns::Integer(0),
                    "Solutions will only be written at every time_step_interval time step."
                    "\nSet to one to output at every time step."
                    "\n Set to zero to output only the final time.");
            }
            prm.leave_subsection();
            
            prm.enter_subsection("verification");
            {
                prm.declare_entry("enabled", "false", Patterns::Bool(),
                "If true, then an exact solution function must be provided."
                "The solution will be compared to the exact solution at every time step,"
                " and this data will be tabulated.");
                
                prm.declare_entry("exact_solution_function_name", "parsed", 
                    Patterns::Selection("parsed"),
                    "This is deprecated.");
                
                prm.enter_subsection("parsed_exact_solution_function");
                {
                    Functions::ParsedFunction<dim>::declare_parameters(prm);    
                }
                prm.leave_subsection();
            }
            prm.leave_subsection();

        }

        /*! Parse a vector valued parameter */
        template<typename ItemType>
        std::vector<ItemType> get_vector(ParameterHandler &prm, std::string parameter_name)
        {
            std::vector<std::string> strings = Utilities::split_string_list(prm.get(parameter_name));
            std::vector<ItemType> items;
            for (auto &string : strings) 
            {
                std::stringstream parser(string);
                ItemType item;
                parser >> item;
                items.push_back(item);
            }
            return items;
        }    
        
        /*! Read only the parameters needed for instantiating a Peclet::Peclet */
        Meta read_meta_parameters(const std::string parameter_file="")
        {
            Meta mp;
            
            ParameterHandler prm;
            declare<1>(prm);
            
            if (parameter_file != "")
            {
                prm.read_input(parameter_file);    
            }
            
            prm.enter_subsection("meta");
            {
                mp.dim = prm.get_integer("dim");  
            }
            prm.leave_subsection();

            return mp;
        }
        
        /*! Structure all parameters from the input file which is read by dealii::ParameterHandler 
        
        This is where parameters in Peclet are handled somewhat differently than most deal.II applications. We only use dealii::ParameterHandler initially for parsing the parameter input file. We immediatley contain the data in a custom data structure. Working with this structure is much more convenient than working with the ParameterHandler throughout the rest of the code.
        
        */
        template <int dim>
        StructuredParameters read(
                const std::string parameter_file,
                Functions::ParsedFunction<dim> &parsed_velocity_function,
                Functions::ParsedFunction<dim> &parsed_diffusivity_function,
                Functions::ParsedFunction<dim> &parsed_source_function,
                Functions::ParsedFunction<dim> &parsed_boundary_function,
                Functions::ParsedFunction<dim> &parsed_exact_solution_function,
                Functions::ParsedFunction<dim> &parsed_initial_values_function)
        {

            StructuredParameters params;
            
            ParameterHandler prm;
            Parameters::declare<dim>(prm);

            if (parameter_file != "")
            {
                prm.read_input(parameter_file);    
            }
            
            // Print a log file of all the ParameterHandler parameters
            std::ofstream parameter_log_file("used_parameters.prm");
            assert(parameter_log_file.good());
            prm.print_parameters(parameter_log_file, ParameterHandler::Text);
            
            prm.enter_subsection("geometry");
            {
                params.geometry.grid_name = prm.get("grid_name");
                params.geometry.sizes = Parameters::get_vector<double>(prm, "sizes");
                params.geometry.transformations = 
                    Parameters::get_vector<double>(prm, "transformations");    
            }
            prm.leave_subsection();
            
            
            prm.enter_subsection("parsed_velocity_function");
            {
                parsed_velocity_function.parse_parameters(prm);    
            }
            prm.leave_subsection();
            
            
            prm.enter_subsection("parsed_diffusivity_function");
            {
                parsed_diffusivity_function.parse_parameters(prm);    
            }
            prm.leave_subsection();

            
            prm.enter_subsection("parsed_source_function");
            {
                parsed_source_function.parse_parameters(prm);
            }
            prm.leave_subsection();
                
            
            prm.enter_subsection("verification");
            {
                params.verification.enabled = prm.get_bool("enabled");
                
                params.verification.exact_solution_function_name = 
                        prm.get("exact_solution_function_name");
                        
                prm.enter_subsection("parsed_exact_solution_function");
                {
                    parsed_exact_solution_function.parse_parameters(prm);    
                }
                prm.leave_subsection();
            }
            prm.leave_subsection();
            
            prm.enter_subsection("boundary_conditions");
            {
                params.boundary_conditions.implementation_types = 
                    Parameters::get_vector<std::string>(prm, "implementation_types");
                params.boundary_conditions.function_names = 
                    Parameters::get_vector<std::string>(prm, "function_names");
                
                std::vector<double> vector = 
                    Parameters::get_vector<double>(prm, "function_double_arguments");
                
                for (auto v : vector)
                {
                    params.boundary_conditions.function_double_arguments.push_back(v);
                }

                prm.enter_subsection("parsed_function");
                {
                    parsed_boundary_function.parse_parameters(prm);
                }
                prm.leave_subsection();
                
            }    
            prm.leave_subsection();
            
            prm.enter_subsection("initial_values");
            {               
                params.initial_values.function_name = prm.get("function_name"); 
                
                prm.enter_subsection("parsed_function");
                {
                    parsed_initial_values_function.parse_parameters(prm);
                }
                prm.leave_subsection();
              
            }    
            prm.leave_subsection();
            
            
            prm.enter_subsection("refinement");
            {
                
                params.refinement.initial_global_cycles = prm.get_integer("initial_global_cycles");
                params.refinement.initial_boundary_cycles = prm.get_integer("initial_boundary_cycles");
                params.refinement.boundaries_to_refine = 
                    Parameters::get_vector<unsigned int>(prm, "boundaries_to_refine");
                
                prm.enter_subsection("adaptive");
                {
                    params.refinement.adaptive.initial_cycles = prm.get_integer("initial_cycles");
                    params.refinement.adaptive.max_level = prm.get_integer("max_level");
                    params.refinement.adaptive.max_cells = prm.get_integer("max_cells");
                    params.refinement.adaptive.interval = prm.get_integer("interval");
                    params.refinement.adaptive.cycles_at_interval = prm.get_integer("cycles_at_interval");
                    params.refinement.adaptive.refine_fraction = prm.get_double("refine_fraction");
                    params.refinement.adaptive.coarsen_fraction = prm.get_double("coarsen_fraction");    
                }        
                
                prm.leave_subsection();
                
            }
            prm.leave_subsection();
                
                
            prm.enter_subsection("time");
            {
                params.time.end_time = prm.get_double("end_time");
                params.time.step_size = prm.get_double("step_size");
                params.time.global_refinement_levels = 
                    prm.get_integer("global_refinement_levels");
                params.time.semi_implicit_theta = prm.get_double("semi_implicit_theta");
                params.time.stop_when_steady = prm.get_bool("stop_when_steady");
            }    
            prm.leave_subsection();
            
            
            prm.enter_subsection("solver");
            {
                params.solver.method = prm.get("method");
                params.solver.max_iterations = prm.get_integer("max_iterations");
                params.solver.tolerance = prm.get_double("tolerance");
                params.solver.normalize_tolerance = prm.get_bool("normalize_tolerance");
            }    
            prm.leave_subsection(); 
            
            prm.enter_subsection("output");
            {
                params.output.write_solution_vtk = prm.get_bool("write_solution_vtk");
                params.output.write_solution_table = prm.get_bool("write_solution_table");
                params.output.time_step_interval = prm.get_integer("time_step_interval");
            }
            prm.leave_subsection();
            
            return params;
        }

        
    }    

}

#endif
