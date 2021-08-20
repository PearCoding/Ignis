set(SRC 
    main.cpp
)
  
add_executable(ig_api_collector ${SRC})
target_compile_definitions(ig_api_collector PUBLIC "$<$<CONFIG:Debug>:IG_DEBUG>")
target_compile_features(ig_api_collector PUBLIC cxx_std_17)
