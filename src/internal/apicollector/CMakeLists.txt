set(SRC 
    main.cpp
)
  
add_executable(ig_api_collector ${SRC})
target_link_libraries(ig_api_collector PUBLIC std::filesystem)
target_compile_definitions(ig_api_collector PUBLIC "$<$<CONFIG:Debug>:IG_DEBUG>")
target_compile_features(ig_api_collector PUBLIC cxx_std_17)
ig_add_extra_options(ig_api_collector)
