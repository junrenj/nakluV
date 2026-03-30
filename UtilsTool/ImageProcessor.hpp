#pragma once

#include "CubeExecute.hpp"

class UImageProcessor
{
public:
    static void GGXProcess(CubeExecute& CubeExe, CubeExecute::Configuration& configuration, float Roughness, float InOutRatio, const std::string& OutputFileName);
    static void IrradianceProcess(CubeExecute& CubeExe, CubeExecute::Configuration& configuration, size_t OutputSize, const std::string& OutputFileName);
    static void BRDFLUTProcess(CubeExecute& CubeExe, CubeExecute::Configuration& configuration, const std::string& OutputPath);
};
