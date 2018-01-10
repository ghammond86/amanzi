#  -*- mode: cmake -*-

#
# Build TPL: NetCDF-Fortran
# 
# --- Define all the directories and common external project flags
define_external_project_args(NetCDF_Fortran TARGET netcdf-fortran
                             DEPENDS NetCDF)

# add version version to the autogenerated tpl_versions.h file
amanzi_tpl_version_write(FILENAME ${TPL_VERSIONS_INCLUDE_FILE}
  PREFIX NetCDF_Fortran
  VERSION ${NetCDF_Fortran_VERSION_MAJOR} ${NetCDF_Fortran_VERSION_MINOR} ${NetCDF_Fortran_VERSION_PATCH})
  
# --- Patch original code
set(NetCDF_Fortran_patch_file netcdf-fortran-osx.patch)
set(NetCDF_Fortran_sh_patch ${NetCDF_Fortran_prefix_dir}/netcdf-fortran-patch-step.sh)
configure_file(${SuperBuild_TEMPLATE_FILES_DIR}/netcdf-fortran-patch-step.sh.in
               ${NetCDF_Fortran_sh_patch}
               @ONLY)

# configure the CMake patch step
set(NetCDF_Fortran_cmake_patch ${NetCDF_Fortran_prefix_dir}/netcdf-fortran-patch-step.cmake)
configure_file(${SuperBuild_TEMPLATE_FILES_DIR}/netcdf-fortran-patch-step.cmake.in
               ${NetCDF_Fortran_cmake_patch}
               @ONLY)

set(NetCDF_Fortran_PATCH_COMMAND ${CMAKE_COMMAND} -P ${NetCDF_Fortran_cmake_patch})

# --- Define the configure command
set(NetCDF_Fortran_CMAKE_CACHE_ARGS "-DCMAKE_INSTALL_PREFIX:FILEPATH=${TPL_INSTALL_PREFIX}")
list(APPEND NetCDF_Fortran_CMAKE_CACHE_ARGS "-DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}")
list(APPEND NetCDF_Fortran_CMAKE_CACHE_ARGS "-DBUILD_SHARED_LIBS:BOOL=${BUILD_SHARED_LIBS}")
list(APPEND NetCDF_Fortran_CMAKE_CACHE_ARGS "-DENABLE_TESTS:BOOL=FALSE")

# --- Add external project build 
ExternalProject_Add(${NetCDF_Fortran_BUILD_TARGET}
                    DEPENDS ${NetCDF_Fortran_PACKAGE_DEPENDS} # Package dependency target
                    TMP_DIR ${NetCDF_Fortran_tmp_dir}         # Temporary files directory
                    STAMP_DIR ${NetCDF_Fortran_stamp_dir}     # Timestamp and log directory
                    # -- Downloads
                    DOWNLOAD_DIR ${TPL_DOWNLOAD_DIR}
                    URL ${NetCDF_Fortran_URL}                 # URL may be a web site OR a local file
                    URL_MD5 ${NetCDF_Fortran_MD5_SUM}         # md5sum of the archive file
                    DOWNLOAD_NAME ${NetCDF_Fortran_SAVEAS_FILE}  # file name to store
                    PATCH_COMMAND ${NetCDF_Fortran_PATCH_COMMAND} 
                    # -- Configure
                    SOURCE_DIR ${NetCDF_Fortran_source_dir}
                    CMAKE_CACHE_ARGS ${AMANZI_CMAKE_CACHE_ARGS}   # Global definitions from root CMakeList
                                     ${NetCDF_Fortran_CMAKE_CACHE_ARGS}
                                     -DCMAKE_C_FLAGS:STRING=${Amanzi_COMMON_CFLAGS}  # Ensure uniform build
                                     -DCMAKE_C_COMPILER:FILEPATH=${CMAKE_C_COMPILER}
                                     -DCMAKE_CXX_FLAGS:STRING=${Amanzi_COMMON_CXXFLAGS}
                                     -DCMAKE_CXX_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER}
                                     -DCMAKE_Fortran_FLAGS:STRING=${Amanzi_COMMON_FCFLAGS}
                                     -DCMAKE_Fortran_COMPILER:FILEPATH=${CMAKE_Fortran_COMPILER}
                    # -- Build
                    BINARY_DIR      ${NetCDF_Fortran_build_dir}  
                    BUILD_COMMAND   $(MAKE)                       # enables parallel builds through make
                    BUILD_IN_SOURCE ${NetCDF_Fortran_BUILD_IN_SOURCE}
                    # -- Install
                    INSTALL_DIR ${TPL_INSTALL_PREFIX}
                    # -- Output control
                    ${NetCDF_Fortran_logging_args})


# --- Useful variables for packages that depend on NetCDF (Trilinos)
set(NetCDF_FORTRAN_DIR ${TPL_INSTALL_PREFIX})
