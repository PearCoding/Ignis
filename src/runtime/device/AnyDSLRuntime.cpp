#include "AnyDSLRuntime.h"
#include "Logger.h"
#include "RuntimeInfo.h"

namespace IG {

static void anydslLogCallback(AnyDSLLogReportLevelFlags flags, const char* pMessage, void* /*pUserData*/)
{
    if (AnyDSL_CHECK_BIT(flags, AnyDSL_LOG_REPORT_LEVEL_TRACE_BIT)) {
        IG_LOG(L_DEBUG) << "TRACE: " << pMessage << std::endl;
    } else if (AnyDSL_CHECK_BIT(flags, AnyDSL_LOG_REPORT_LEVEL_DEBUG_BIT)) {
        IG_LOG(L_DEBUG) << pMessage << std::endl;
    } else if (AnyDSL_CHECK_BIT(flags, AnyDSL_LOG_REPORT_LEVEL_WARNING_BIT)) {
        IG_LOG(L_WARNING) << pMessage << std::endl;
    } else if (AnyDSL_CHECK_BIT(flags, AnyDSL_LOG_REPORT_LEVEL_ERROR_BIT)) {
        IG_LOG(L_ERROR) << pMessage << std::endl;
    } else {
        IG_LOG(L_INFO) << pMessage << std::endl;
    }
}

static AnyDSLLogReportCallback sLogReportClb;
void anydslInit()
{
    AnyDSLLogReportCallbackCreateInfo info = {
        AnyDSL_STRUCTURE_TYPE_LOG_REPORT_CALLBACK_CREATE_INFO,
        nullptr,
        AnyDSL_LOG_REPORT_LEVEL_MAX_ENUM,
        anydslLogCallback,
        nullptr
    };

    anydslCreateLogReportCallback(&info, &sLogReportClb);

    const auto cache_dir = RuntimeInfo::cacheDirectory();

    AnyDSLOptions options = {
        AnyDSL_STRUCTURE_TYPE_OPTIONS,
        nullptr,
        cache_dir.c_str()
    };
    anydslSetOptions(&options);
}

static std::array<const char*, 4> sDeviceTypes = {
    "Host",
    "Cuda",
    "OpenCL",
    "HSA"
};

void anydslInspect()
{
    // Only inspect if we actually display it.
    if (IG_LOGGER.verbosity() == L_DEBUG)
        return;

    size_t count = 0;
    if (anydslEnumerateDevices(&count, nullptr) != AnyDSL_SUCCESS) {
        IG_LOG(L_ERROR) << "AnyDSL: Could not enumerate devices." << std::endl;
        return;
    }

    if (count == 0) {
        IG_LOG(L_WARNING) << "No AnyDSL supported devices found." << std::endl;
        return;
    }

    IG_LOG(L_DEBUG) << "Found " << count << " AnyDSL devices" << std::endl;

    std::vector<AnyDSLDeviceInfo> infos(count);
    for (size_t i = 0; i < count; ++i) {
        infos[i].sType = AnyDSL_STRUCTURE_TYPE_DEVICE_INFO;
        infos[i].pNext = nullptr;
    }

    if (anydslEnumerateDevices(&count, infos.data()) != AnyDSL_SUCCESS) {
        IG_LOG(L_ERROR) << "AnyDSL: Could not enumerate devices." << std::endl;
        return;
    }

    for (size_t i = 0; i < count; ++i) {
        std::stringstream stream;
        stream << "(" << i << ") => " << infos[i].name << std::endl
               << " - Type     : " << sDeviceTypes.at(infos[i].deviceType) << std::endl
               << " - Number   : " << infos[i].deviceNumber << std::endl
               << " - Version  : " << infos[i].version << std::endl
               << " - Total MB : " << (infos[i].totalMemory / (1024 * 1024)) << std::endl
               << " - Is Host  : " << (infos[i].isHost != AnyDSL_FALSE ? "Yes" : "No") << std::endl;

        AnyDSLGetDeviceRequest req = {
            AnyDSL_STRUCTURE_TYPE_GET_DEVICE_REQUEST,
            nullptr,
            infos[i].deviceType,
            infos[i].deviceNumber
        };
        AnyDSLDevice device;
        if (anydslGetDevice(&req, &device) != AnyDSL_SUCCESS) {
            IG_LOG(L_ERROR) << "AnyDSL: Could not get device " << i << "." << std::endl;
            return;
        }

        if (infos[i].deviceType == AnyDSL_DEVICE_CUDA) {
            AnyDSLDeviceFeaturesCuda cudaFeatures;
            cudaFeatures.sType = AnyDSL_STRUCTURE_TYPE_DEVICE_FEATURES_CUDA;
            cudaFeatures.pNext = nullptr;

            AnyDSLDeviceFeatures features;
            features.sType = AnyDSL_STRUCTURE_TYPE_DEVICE_FEATURES;
            features.pNext = &cudaFeatures;

            if (anydslGetDeviceFeatures(device, &features) != AnyDSL_SUCCESS) {
                IG_LOG(L_ERROR) << "AnyDSL: Could not get features for device " << i << "." << std::endl;
                return;
            }

            stream << " - Features >" << std::endl
                   << "   - Max Block                : [" << cudaFeatures.maxBlockDim[0] << ", " << cudaFeatures.maxBlockDim[1] << ", " << cudaFeatures.maxBlockDim[2] << "]" << std::endl
                   << "   - Max Grid                 : [" << cudaFeatures.maxGridDim[0] << ", " << cudaFeatures.maxGridDim[1] << ", " << cudaFeatures.maxGridDim[2] << "]" << std::endl
                   << "   - Max Reg Per Block        : " << cudaFeatures.maxRegistersPerBlock << std::endl
                   << "   - Max Shared Mem Per Block : " << cudaFeatures.maxSharedMemPerBlock << "b" << std::endl
                   << "   - Free MB                  : " << (cudaFeatures.freeMemory / (1024 * 1024)) << std::endl;
        }

        IG_LOG(L_DEBUG) << stream.str();
    }
}

bool checkAnyDSLResult(AnyDSLResult result, const char* func, const char* file, int line)
{
    if (result >= 0)
        return true;

    const char* errPrefix = "Unknown";
    if (result == AnyDSL_INCOMPLETE)
        errPrefix = "Incomplete";
    else if (result == AnyDSL_NOT_AVAILABLE)
        errPrefix = "Not available";
    else if (result == AnyDSL_NOT_SUPPORTED)
        errPrefix = "Not supported";
    else if (result == AnyDSL_OUT_OF_HOST_MEMORY)
        errPrefix = "Out of host memory";
    else if (result == AnyDSL_OUT_OF_DEVICE_MEMORY)
        errPrefix = "Out of device memory";
    else if (result == AnyDSL_INVALID_POINTER)
        errPrefix = "Invalid pointer";
    else if (result == AnyDSL_INVALID_VALUE)
        errPrefix = "Invalid value";
    else if (result == AnyDSL_INVALID_HANDLE)
        errPrefix = "Invalid handle";
    else if (result == AnyDSL_DEVICE_MISSMATCH)
        errPrefix = "Device missmatch";
    else if (result == AnyDSL_PLATFORM_ERROR)
        errPrefix = "Platform error";
    else if (result == AnyDSL_JIT_ERROR)
        errPrefix = "JIT error";
    else if (result == AnyDSL_JIT_NO_FUNCTION)
        errPrefix = "JIT not function";

    IG_LOG(L_ERROR) << "AnyDSL: " << errPrefix << " [" << func << ", " << file << ", " << line << "]" << std::endl;

    return false;
}
}; // namespace IG