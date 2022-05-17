cmake_minimum_required (VERSION 3.16)
project (memory_resource)

include (CheckCXXSymbolExists)
check_cxx_symbol_exists ("_LIBCPP_VERSION" "ciso646" HAS_LIBCPP) # Would use <version> but not all platforms have it
if (HAS_LIBCPP)
  add_library (memory_resource STATIC)
  target_compile_features (memory_resource PRIVATE cxx_std_20)
  target_compile_options (memory_resource PRIVATE -Wall -Wextra -pedantic)
  target_include_directories (memory_resource SYSTEM PUBLIC include)
  target_sources (memory_resource PRIVATE
      include/memory_resource
      include/memory_resource.hpp
      src/memory_resource.cpp
  )
else ()
  add_library (memory_resource INTERFACE)
endif ()

add_library (memory_resource::memory_resource ALIAS memory_resource)
