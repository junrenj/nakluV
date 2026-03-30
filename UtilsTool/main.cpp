
#include "ImageProcessor.hpp"
#include <iostream>
#include <fstream>


int main(int argc, char **argv)
{
    //main wrapped in a try-catch so we can print some debug info about uncaught exceptions:
	try 
	{
		//configure application:
		CubeExecute::Configuration configuration;

		configuration.application_info = VkApplicationInfo
		{
			.pApplicationName = "Image Processor",
			.applicationVersion = VK_MAKE_VERSION(0,0,0),
			.pEngineName = "Unknown",
			.engineVersion = VK_MAKE_VERSION(0,0,0),
			.apiVersion = VK_API_VERSION_1_3
		};

		bool print_usage = false;

		try 
		{
			configuration.parse(argc, argv);
		} 
		catch (std::runtime_error &e) 
		{
			std::cerr << "Failed to parse arguments:\n" << e.what() << std::endl;
			print_usage = true;
		}

        configuration.headless = true;

		if (print_usage) 
		{
			std::cerr << "Usage:" << std::endl;
			CubeExecute::Configuration::usage( [](const char *arg, const char *desc)
			{ 
				std::cerr << "    " << arg << "\n        " << desc << std::endl;
			});
			return 1;
		}

		//loads vulkan library, load device, initializes helpers:
		CubeExecute CubeExe(configuration);

		if(configuration.ProcessMode == CubeExecute::Configuration::EProcessMode::Cubemap2Irradiance)
		{
			const std::string FileName = configuration.OutImagePath + "_Irradiance" + ".png";
			UImageProcessor::IrradianceProcess(CubeExe, configuration, configuration.IrradianceOutputSize, FileName);
		}
		else if(configuration.ProcessMode == CubeExecute::Configuration::EProcessMode::Cubemap2GGX)
		{
			const uint8_t GGXLevelsCount = configuration.GGXLevelsCount; 
			for (uint8_t i = 1; i <= GGXLevelsCount; i++)
			{
				const std::string FileName = configuration.OutImagePath + "_GGX_" + std::to_string(i) + ".png";
				const float InRoughness = (float)i / (float)GGXLevelsCount;
				const float Ratio = 1.0f / powf(2.0f, i);
				UImageProcessor::GGXProcess(CubeExe, configuration, InRoughness, Ratio, FileName);
			}
			
		}
		else if(configuration.ProcessMode == CubeExecute::Configuration::EProcessMode::BrdfLUT)
		{
			const std::string FileName = configuration.OutImagePath + ".hdr"; 	// HDR Format
			UImageProcessor::BRDFLUTProcess(CubeExe, configuration, FileName);
		}

		std::cout << "Computing: done." << std::endl;
		
	} 
	catch (std::exception &e) 
	{
		std::cerr << "Exception: " << e.what() << std::endl;
		return 1;
	}
    return 0;
}

