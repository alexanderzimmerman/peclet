#include "pde_model.h"

int main(int argc, char* argv[])
{
    try
    {   
        
        std::string parameter_input_file_path = "";
        
        if (argc == 2)
        {
            parameter_input_file_path = argv[1];
        }
        
        PDE::Parameters::Meta mp = PDE::Parameters::read_meta_parameters(parameter_input_file_path);
        
         /*
        Only a compile time constant can be used as the template arguments to insantiate the model,
        so we must instantiate each possible dimensionality. This is virtually free, since of course
        data will only be generated for one of these models.
        */
        PDE::Model<1> model_1D;
        PDE::Model<2> model_2D;
        PDE::Model<3> model_3D;

        switch (mp.dim)
        {
            case 1:
                model_1D.run(parameter_input_file_path);
                break;
            case 2:
                model_2D.run(parameter_input_file_path);
                break;
            case 3:
                model_3D.run(parameter_input_file_path);
                break;
        }

    }
    catch (std::exception &exc)
    {
        std::cerr << std::endl << std::endl
              << "----------------------------------------------------"
              << std::endl;
        std::cerr << "Exception on processing: " << std::endl << exc.what()
              << std::endl << "Aborting!" << std::endl
              << "----------------------------------------------------"
              << std::endl;
        return 1;
    }
    catch (...)
    {
        std::cerr << std::endl << std::endl
              << "----------------------------------------------------"
              << std::endl;
        std::cerr << "Unknown exception!" << std::endl << "Aborting!"
              << std::endl
              << "----------------------------------------------------"
              << std::endl;
        return 1;
    }
    return 0;
}