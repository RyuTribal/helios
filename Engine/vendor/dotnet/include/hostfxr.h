#pragma once

#include <cstdint>

#ifdef _WIN32
    using char_t = wchar_t;
#else
    using char_t = char;
#endif

// Opaque handle to an initialized host context
using hostfxr_handle = void*;

// Parameters for hostfxr_initialize_for_runtime_config
struct hostfxr_initialize_parameters
{
    size_t size;
    const char_t* host_path;
    const char_t* dotnet_root;
};

// Delegate types from hostfxr_get_runtime_delegate
enum hostfxr_delegate_type
{
    hdt_com_activation                  = 0,
    hdt_load_in_memory_assembly         = 1,
    hdt_winrt_activation                = 2,
    hdt_com_register                    = 3,
    hdt_com_unregister                  = 4,
    hdt_load_assembly_and_get_function_pointer = 5,
    hdt_get_function_pointer            = 6,
};

// Function pointer typedefs for the hostfxr exports we load dynamically
using hostfxr_initialize_for_runtime_config_fn = int32_t(*)(
    const char_t* runtime_config_path,
    const hostfxr_initialize_parameters* parameters,
    hostfxr_handle* host_context_handle);

using hostfxr_get_runtime_delegate_fn = int32_t(*)(
    const hostfxr_handle host_context_handle,
    int32_t type,
    void** delegate);

using hostfxr_close_fn = int32_t(*)(
    const hostfxr_handle host_context_handle);
