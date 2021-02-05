#  -*- mode: cmake -*-

#
# Build TPL:  HYPRE 
#    

# --- Define all the directories and common external project flags
define_external_project_args(HYPRE 
                             TARGET hypre
                             DEPENDS SuperLU SuperLUDist)

# add version version to the autogenerated tpl_versions.h file
amanzi_tpl_version_write(FILENAME ${TPL_VERSIONS_INCLUDE_FILE}
  PREFIX HYPRE
  VERSION ${HYPRE_VERSION_MAJOR} ${HYPRE_VERSION_MINOR} ${HYPRE_VERSION_PATCH})


# --- Define configure parameters

# Disable OpenMP with HYPRE for now
# Is OpenMP available
# if (ENABLE_OpenMP)
#   find_package(OpenMP)

set(hypre_openmp_opt)
# if (OPENMP_FOUND)
#   set(hypre_openmp_opt "-DHYPRE_USING_OPENMP:BOOL=TRUE")
# endif()
#else()
#set(hypre_openmp_opt "-DHYPRE_USING_OPENMP:BOOL=FALSE")
#set(hypre_openmp_opt "--with-openmp")
#endif()

# Locate LAPACK and BLAS
set(hypre_blas_opt)
find_package(BLAS)
if (BLAS_FOUND)
  set(hypre_blas_opt "--with-blas")
endif()

set(hypre_lapack_opt)
find_package(LAPACK)
if (LAPACK_FOUND)
  set(hypre_lapack_opt "--with-lapack")
endif()

set(hypre_kokkos_cuda)
set(CUDA_HOME)
set(Hypre_CUDA_SM)
if(ENABLE_KOKKOS_CUDA)
  find_package(CUDA REQUIRED)
  set(CUDA_HOME ${CUDA_INCLUDE_DIRS}/..)
  message(STATUS "CUDA_HOME: ${CUDA_HOME}")
  set(Hypre_CUDA_SM 70)
  set(hypre_kokkos_cuda "--with-cuda" "--enable-cusparse" "--enable-unified-memory")
endif()

# set(hypre_fortran_opt -"--disable-fortran)

# Locate SuperLU and SuperLUDist
set(hypre_superlu_opt "--with-superlu" 
                      "--with-superlu-include=${TPL_INSTALL_PREFIX}/include"
                      "--with-superlu-lib=${SuperLU_LIBRARY}"
                      "--with-dsuperlu"
                      "--with-dsuperlu-include=${TPL_INSTALL_PREFIX}/include"
                      "--with-dsuperlu-lib=${SuperLUDist_LIBRARY}")

# shared/static libraries (shared FEI is broken in HYPRE)
set(hypre_shared_opt "no")
if (BUILD_SHARED_LIBS)
  set(hypre_shared_opt "yes")
endif()

# --- Set the name of the patch
set(HYPRE_patch_file hypre-superlu.patch)
# --- Configure the bash patch script
set(HYPRE_sh_patch ${HYPRE_prefix_dir}/hypre-patch-step.sh)
configure_file(${SuperBuild_TEMPLATE_FILES_DIR}/hypre-patch-step.sh.in
               ${HYPRE_sh_patch}
               @ONLY)
# --- Configure the CMake patch step
set(HYPRE_cmake_patch ${HYPRE_prefix_dir}/hypre-patch-step.cmake)
configure_file(${SuperBuild_TEMPLATE_FILES_DIR}/hypre-patch-step.cmake.in
               ${HYPRE_cmake_patch}
               @ONLY)
# --- Set the patch command
set(HYPRE_PATCH_COMMAND ${CMAKE_COMMAND} -P ${HYPRE_cmake_patch})     


# --- Add external project build and tie to the ZLIB build target
ExternalProject_Add(${HYPRE_BUILD_TARGET}
                    DEPENDS   ${HYPRE_PACKAGE_DEPENDS}         # Package dependency target
                    TMP_DIR   ${HYPRE_tmp_dir}                 # Temporary files directory
                    STAMP_DIR ${HYPRE_stamp_dir}               # Timestamp and log directory
                    # -- Download and URL definitions
                    DOWNLOAD_DIR   ${TPL_DOWNLOAD_DIR}          
                    URL            ${HYPRE_URL}                # URL may be a web site OR a local file
                    URL_MD5        ${HYPRE_MD5_SUM}            # md5sum of the archive file
                    DOWNLOAD_NAME  ${HYPRE_SAVEAS_FILE}        # file name to store (if not end of URL)
                    # -- Patch 
                    PATCH_COMMAND  ${HYPRE_PATCH_COMMAND}
                    # -- Configure
                    SOURCE_DIR    ${HYPRE_source_dir}
                    SOURCE_SUBDIR src                          # cmake 3.7+ feature 
                    CONFIGURE_COMMAND
                                 ${HYPRE_source_dir}/src/configure
                                       --prefix=${TPL_INSTALL_PREFIX}
                                       --with-MPI
                                       --enable-shared=${hypre_shared_opt}
                                       ${hypre_openmp_opt}
                                       ${hypre_lapack_opt}
                                       ${hypre_blas_opt}
                                       ${hypre_superlu_opt}
                                       ${hypre_kokkos_cuda}
                                       CC=${CMAKE_CC_COMPILER}
                                       CFLAGS=${Hypre_CC_FLAGS}
                                       CXX=${CMAKE_CXX_COMPILER}
                                       CXXFLAGS=${Hypre_CXX_FLAGS}
                                       CUDA_HOME=${CUDA_HOME}
                                       HYPRE_CUDA_SM=${Hypre_CUDA_SM}               
                    # -- Build
                    BINARY_DIR       ${HYPRE_source_dir}/src        # Build directory 
                    BUILD_COMMAND    $(MAKE)  
                    # -- Install
                    INSTALL_DIR      ${TPL_INSTALL_PREFIX}     # Install directory
      		    INSTALL_COMMAND  $(MAKE) install
                    # -- Output control
                    ${HYPRE_logging_args})


# --- Useful variables that depend on HYPRE
include(BuildLibraryName)
build_library_name(HYPRE HYPRE_LIBRARY APPEND_PATH ${TPL_INSTALL_PREFIX}/lib)
set(HYPRE_INSTALL_PREFIX ${TPL_INSTALL_PREFIX})
