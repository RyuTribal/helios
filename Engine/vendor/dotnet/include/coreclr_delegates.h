#pragma once

#ifdef _WIN32
    using char_t = wchar_t;
#else
    using char_t = char;
#endif

// Delegate type returned by hdt_load_assembly_and_get_function_pointer
using load_assembly_and_get_function_pointer_fn = int(*)(
    const char_t* assembly_path,
    const char_t* type_name,
    const char_t* method_name,
    const char_t* delegate_type_name,
    void* reserved,
    void** delegate);

// Sentinel value indicating the managed method uses [UnmanagedCallersOnly]
#define UNMANAGEDCALLERSONLY_METHOD ((const char_t*)-1)
