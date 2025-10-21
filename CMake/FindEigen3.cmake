# Minimal header-only finder
find_path(EIGEN3_INCLUDE_DIR Eigen/Core
    PATHS
      E:/Transmixr/NeuS2/dependencies/eigen
    PATH_SUFFIXES include/eigen3 eigen3 include
  )
  
  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(Eigen3 REQUIRED_VARS EIGEN3_INCLUDE_DIR)
  
  if(Eigen3_FOUND AND NOT TARGET Eigen3::Eigen)
    add_library(Eigen3::Eigen INTERFACE IMPORTED)
    set_target_properties(Eigen3::Eigen PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${EIGEN3_INCLUDE_DIR}")
  endif()