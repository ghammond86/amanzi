#  -*- mode: cmake -*-

#
# Build TPL: Trilinos
#    
# --- Define all the directories and common external project flags
if (ENABLE_XSDK)
    set(trilinos_depend_projects XSDK SEACAS)
else()
    set(trilinos_depend_projects NetCDF Boost SEACAS SuperLUDist ParMetis)
endif() 

if (ENABLE_HYPRE AND NOT ENABLE_XSDK)
  list(APPEND trilinos_depend_projects HYPRE)
endif()

define_external_project_args(Trilinos
                             TARGET trilinos
                             DEPENDS ${trilinos_depend_projects})

# add version version to the autogenerated tpl_versions.h file
amanzi_tpl_version_write(FILENAME ${TPL_VERSIONS_INCLUDE_FILE}
  PREFIX Trilinos
  VERSION ${Trilinos_VERSION_MAJOR} ${Trilinos_VERSION_MINOR} ${Trilinos_VERSION_PATCH})
  
# --- Define the configuration parameters   

#  - Trilinos Package Configuration

#if(Trilinos_Build_Config_File)
#  message(STATUS "Including Trilinos build configuration file ${Trilinos_Build_Config_File}")
#  if ( NOT EXISTS ${Trilinos_Build_Config_File} )
#    message(FATAL_ERROR "File ${Trilinos_Build_Config_File} does not exist.")
#  endif()
#  include(${Trilinos_Build_Config_File})
#endif()

# List of packages enabled in the Trilinos build
set(Trilinos_PACKAGE_LIST Teuchos)

if (ENABLE_EPETRA)
  list(APPEND Trilinos_PACKAGE_LIST Epetra EpetraExt Amesos Amesos2 Belos NOX Ifpack AztecOO)
  if (ENABLE_Unstructured)
    list(APPEND Trilinos_PACKAGE_LIST ML)
  endif()
endif()

if (ENABLE_KOKKOS)
  list(APPEND Trilinos_PACKAGE_LIST Kokkos KokkosKernels Tpetra Ifpack2 MueLu)
endif()

if (ENABLE_STK_Mesh)
  list(APPEND Trilinos_PACKAGE_LIST STK)
endif()
if (ENABLE_MSTK_Mesh)
  list(APPEND Trilinos_PACKAGE_LIST Zoltan)
endif()

# Generate the Trilinos Package CMake Arguments
set(Trilinos_CMAKE_PACKAGE_ARGS "-DTrilinos_ENABLE_ALL_OPTIONAL_PACKAGES:BOOL=OFF")
foreach(package ${Trilinos_PACKAGE_LIST})
  list(APPEND Trilinos_CMAKE_PACKAGE_ARGS "-DTrilinos_ENABLE_${package}:STRING=ON")
endforeach()

# Add support of parallel LU solvers
list(APPEND Trilinos_CMAKE_PACKAGE_ARGS "-DAmesos2_ENABLE_Basker:BOOL=ON")

# not sure why we don't like Stratimikos...
list(APPEND Trilinos_CMAKE_PACKAGE_ARGS "-DTrilinos_ENABLE_Stratimikos:BOOL=FALSE")

# have already built SEACAS
list(APPEND Trilinos_CMAKE_PACKAGE_ARGS "-DTrilinos_ENABLE_SEACAS:BOOL=FALSE")

# we use ints for GOs in Tpetra only (for now)
if (ENABLE_KOKKOS)
  list(APPEND Trilinos_CMAKE_PACKAGE_ARGS "-DTpetra_INST_INT_INT:BOOL=ON")
  list(APPEND Trilinos_CMAKE_PACKAGE_ARGS "-DTpetra_INST_INT_LONG:BOOL=OFF")
  list(APPEND Trilinos_CMAKE_PACKAGE_ARGS "-DTpetra_INST_INT_LONG_LONG:BOOL=OFF")
  list(APPEND Trilinos_CMAKE_PACKAGE_ARGS "-DXpetra_Epetra_NO_64BIT_GLOBAL_INDICIES:BOOL=ON")
endif()



# Build PyTrilinos if shared
# if (BUILD_SHARED_LIBS)
#   list(APPEND Trilinos_CMAKE_PACKAGE_ARGS "-DTrilinos_ENABLE_PyTrilinos:BOOL=ON")
#endif()

#  - Trilinos TPL Configuration
set(Trilinos_CMAKE_TPL_ARGS)

# MPI
list(APPEND Trilinos_CMAKE_TPL_ARGS "-DTPL_ENABLE_MPI:BOOL=ON")

# Pass the following MPI arguments to Trilinos if they are set 
set(MPI_CMAKE_ARGS DIR EXEC EXEC_NUMPROCS_FLAG EXEC_MAX_NUMPROCS C_COMPILER)
foreach (var ${MPI_CMAKE_ARGS} )
  set(mpi_var "MPI_${var}")
  if ( ${mpi_var} )
    list(APPEND Trilinos_CMAKE_TPL_ARGS "-D${mpi_var}:STRING=${${mpi_var}}")
  endif()
endforeach() 

# BLAS
if (BLAS_LIBRARIES)
  list(APPEND Trilinos_CMAKE_TPL_ARGS
              "-DTPL_ENABLE_BLAS:BOOL=TRUE")
  list(APPEND Trilinos_CMAKE_TPL_ARGS
              "-DTPL_BLAS_LIBRARIES:STRING=${BLAS_LIBRARIES}")
  message(STATUS "Trilinos BLAS libraries: ${BLAS_LIBRARIES}")    
else()
  message(WARNING "BLAS libraies not set. Trilinos will perform search.") 
endif()            
 
# LAPACK
if (LAPACK_LIBRARIES)
  list(APPEND Trilinos_CMAKE_TPL_ARGS
              "-DTPL_LAPACK_LIBRARIES:STRING=${LAPACK_LIBRARIES}")
            message(STATUS "Trilinos LAPACK libraries: ${LAPACK_LIBRARIES}")    
else()
  message(WARNING "LAPACK libraies not set. Trilinos will perform search.") 
endif()

# Boost
list(APPEND Trilinos_CMAKE_TPL_ARGS
            "-DTPL_ENABLE_BoostLib:BOOL=ON" 
            "-DTPL_ENABLE_Boost:BOOL=ON" 
            "-DTPL_ENABLE_GLM:BOOL=OFF" 
            "-DTPL_BoostLib_INCLUDE_DIRS:FILEPATH=${BOOST_ROOT}/include"
            "-DBoostLib_LIBRARY_DIRS:FILEPATH=${BOOST_ROOT}/lib"
            "-DTPL_Boost_INCLUDE_DIRS:FILEPATH=${BOOST_ROOT}/include"
            "-DBoost_LIBRARY_DIRS:FILEPATH=${BOOST_ROOT}/lib")

# NetCDF
list(APPEND Trilinos_CMAKE_TPL_ARGS
            "-DTPL_ENABLE_Netcdf:BOOL=ON"
            "-DTPL_Netcdf_INCLUDE_DIRS:FILEPATH=${NetCDF_INCLUDE_DIRS}"
            "-DTPL_Netcdf_LIBRARIES:STRING=${NetCDF_C_LIBRARIES}")

# HYPRE
if (ENABLE_HYPRE)
  list(APPEND Trilinos_CMAKE_TPL_ARGS
              "-DTPL_ENABLE_HYPRE:BOOL=ON"
              "-DTPL_HYPRE_LIBRARIES:STRING=${HYPRE_LIBRARIES}"
              "-DHYPRE_LIBRARY_DIRS:FILEPATH=${HYPRE_DIR}/lib"
              "-DTPL_HYPRE_INCLUDE_DIRS:FILEPATH=${HYPRE_DIR}/include")
endif()

# SuperLUDist
if (ENABLE_SUPERLU)
  list(APPEND Trilinos_CMAKE_TPL_ARGS
              "-DTPL_ENABLE_SuperLUDist:BOOL=ON"
              "-DTPL_SuperLUDist_INCLUDE_DIRS:FILEPATH=${SuperLUDist_DIR}/include"
              "-DTPL_SuperLUDist_LIBRARIES:STRING=${SuperLUDist_LIBRARY}")
endif()

# ParMETIS
list(APPEND Trilinos_CMAKE_TPL_ARGS
            "-DTPL_ENABLE_ParMETIS:BOOL=ON"
            "-DTPL_ParMETIS_INCLUDE_DIRS:FILEPATH=${ParMetis_DIR}/include"
            "-DTPL_ParMETIS_LIBRARIES:STRING=${ParMetis_LIBRARIES}")

# - Additional Trilinos CMake Arguments
# Attempts to make Trilinos compile faster go here...          
set(Trilinos_CMAKE_EXTRA_ARGS
    "-DTrilinos_VERBOSE_CONFIGURE:BOOL=ON"
    "-DTrilinos_ENABLE_TESTS:BOOL=OFF"
    "-DTpetra_ENABLE_TESTS:BOOL=OFF"
    "-DTpetra_ENABLE_EXAMPLES:BOOL=OFF"
    "-DTrilinos_ENABLE_EXPLICIT_INSTANTIATION:BOOL=ON"
    "-DCMAKE_VERBOSE_MAKEFILE:BOOL=ON"
    "-DNOX_ENABLE_ABSTRACT_IMPLEMENTATION_THYRA:BOOL=OFF"
    "-DNOX_ENABLE_THYRA_EPETRA_ADAPTERS:BOOL=OFF"    
    "-DTpetra_ENABLE_DEPRECATED_CODE:BOOL=OFF"
    )

if (Trilinos_BUILD_TYPE)
else()
  set(Trilinos_BUILD_TYPE "${CMAKE_BUILD_TYPE}")
endif()

if (${Trilinos_BUILD_TYPE} STREQUAL "Debug")
  list(APPEND Trilinos_CMAKE_EXTRA_ARGS "-DEpetra_ENABLE_FATAL_MESSAGES:BOOL=ON")
  list(APPEND Trilinos_CMAKE_EXTRA_ARGS "-DTpetra_ENABLE_DEBUG:BOOL=ON")
endif()

if (BUILD_SHARED_LIBS)
  list(APPEND Trilinos_CMAKE_EXTRA_ARGS "-DBUILD_SHARED_LIBS:BOOL=${BUILD_SHARED_LIBS}")
endif()


#  - Add CMake configuration file
if (Trilinos_Build_Config_File)
    list(APPEND Trilinos_Config_File_ARGS
        "-C${Trilinos_Build_Config_File}")
    message(STATUS "Will add ${Trilinos_Build_Config_File} to the Trilinos configure")    
    message(DEBUG "Trilinos_CMAKE_EXTRA_ARGS = ${Trilinos_CMAKE_EXTRA_ARGS}")
endif()    

set(Trilinos_CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
set(Trilinos_CMAKE_C_FLAGS ${CMAKE_C_FLAGS})
set(Trilinos_CMAKE_Fortran_FLAGS ${CMAKE_Fortran_FLAGS})
message(DEBUG "Trilinos_CMAKE_CXX_FLAGS = ${Trilinos_CMAKE_CXX_FLAGS}")

if (ENABLE_KOKKOS)
  # - Architecture Args.... these will need work
  set(Trilinos_CMAKE_ARCH_ARGS
    "-DKokkos_ENABLE_Serial:BOOL=ON"
    "-DKokkos_ENABLE_Pthread:BOOL=OFF"
    )

  # By default compiler with the standard mpi compiler
  set(Trilinos_CXX_COMPILER ${CMAKE_CXX_COMPILER})
  message(STATUS "Trilinos COMPILER: ${Trilinos_CXX_COMPILER}")

  if ( "${AMANZI_ARCH}" STREQUAL "Summit" )
    message("AMANZI_ARCH: : ${AMANZI_ARCH}")
    if(NOT DEFINED ENV{CUDA_LAUNCH_BLOCKING})
      message(FATAL_ERROR "Environment CUDA_LAUNCH_BLOCKING have to be set to 1 to continue")
    endif()
    message(STATUS "NVCC_WRAPPER_DEFAULT_COMPILER=${NVCC_WRAPPER_DEFAULT_COMPILER}")
    set(NVCC_WRAPPER_PATH "${Trilinos_source_dir}/packages/kokkos/bin/nvcc_wrapper")
    set(Trilinos_CMAKE_CXX_FLAGS "${Trilinos_CMAKE_CXX_FLAGS} \
        -Wno-deprecated-declarations -lineinfo \
        -Xcudafe --diag_suppress=conversion_function_not_usable \
        -Xcudafe --diag_suppress=cc_clobber_ignored \
        -Xcudafe --diag_suppress=code_is_unreachable")
    message("WRAPPER: ${NVCC_WRAPPER_PATH}")
    message("COMPILER: ${CMAKE_CXX_COMPILER}")
    message("FLAGS: ${Trilinos_CMAKE_CXX_FLAGS}")
    list(APPEND Trilinos_CMAKE_ARCH_ARGS
      "-DTPL_ENABLE_CUDA:BOOL=ON"
      "-DKokkos_ENABLE_Cuda:BOOL=ON"
      "-DKokkos_ENABLE_Cuda_UVM:BOOL=ON"
      "-DKokkos_ENABLE_Cuda_Lambda:BOOL=ON"
      "-DKOKKOS_ARCH:STRING=Power9;Volta70"
      "-DKokkos_ENABLE_OpenMP:BOOL=OFF")
    # Change the default compiler for Trilinos to use nvcc_wrapper
    set(Trilinos_CXX_COMPILER ${NVCC_WRAPPER_PATH})

  else()
    list(APPEND Trilinos_CMAKE_ARCH_ARGS
      "-DKokkos_ENABLE_CUDA:BOOL=OFF"
      "-DKokkos_ENABLE_OpenMP:BOOL=OFF")
  endif()
endif()

 
#  - Final Trilinos CMake Arguments
set(Trilinos_CMAKE_ARGS
  ${Trilinos_CMAKE_PACKAGE_ARGS}
  ${Trilinos_CMAKE_TPL_ARGS}
  ${Trilinos_CMAKE_ARCH_ARGS}
  ${Trilinos_CMAKE_EXTRA_ARGS}
  )


#  --- Define the Trilinos patch step
#

# Trilinos patches
set(ENABLE_Trilinos_Patch ON)
if (ENABLE_Trilinos_Patch)
  set(Trilinos_patch_file trilinos-duplicate-parameters.patch)
  configure_file(${SuperBuild_TEMPLATE_FILES_DIR}/trilinos-patch-step.sh.in
                 ${Trilinos_prefix_dir}/trilinos-patch-step.sh
                 @ONLY)
  set(Trilinos_PATCH_COMMAND bash ${Trilinos_prefix_dir}/trilinos-patch-step.sh)
  message(STATUS "Applying trilinos patches")
else()
  set(Trilinos_PATCH_COMMAND)
  message(STATUS "Patch NOT APPLIED for trilinos")
endif()

# --- Define the Trilinos location
set(Trilinos_install_dir ${TPL_INSTALL_PREFIX}/${Trilinos_BUILD_TARGET}-${Trilinos_VERSION})

# --- Add external project build and tie to the Trilinos build target
ExternalProject_Add(${Trilinos_BUILD_TARGET}
                    DEPENDS   ${Trilinos_PACKAGE_DEPENDS}             # Package dependency target
                    TMP_DIR   ${Trilinos_tmp_dir}                     # Temporary files directory
                    STAMP_DIR ${Trilinos_stamp_dir}                   # Timestamp and log directory
                    # -- Download and URL definitions
                    DOWNLOAD_DIR   ${TPL_DOWNLOAD_DIR}                # Download directory
                    GIT_REPOSITORY ${Trilinos_GIT_REPOSITORY}              
                    GIT_TAG        ${Trilinos_GIT_TAG}      
                    # -- Update (one way to skip this step is use null command)
                    UPDATE_COMMAND ""
                    # -- Patch
                    PATCH_COMMAND ${Trilinos_PATCH_COMMAND}
                    # -- Configure
                    SOURCE_DIR    ${Trilinos_source_dir}           # Source directory
                    CMAKE_ARGS        ${Trilinos_Config_File_ARGS}
                    CMAKE_CACHE_ARGS  ${AMANZI_CMAKE_CACHE_ARGS}   # Ensure uniform build
                                      ${Trilinos_CMAKE_ARGS} 
			              -DCMAKE_CXX_COMPILER:STRING=${Trilinos_CXX_COMPILER}
                                      -DCMAKE_CXX_FLAGS:STRING=${Trilinos_CMAKE_CXX_FLAGS}
                                      -DCMAKE_C_COMPILER:FILEPATH=${CMAKE_C_COMPILER}
                                      -DCMAKE_C_FLAGS:STRING=${Trilinos_CMAKE_C_FLAGS}
                                      -DCMAKE_Fortran_COMPILER:FILEPATH=${CMAKE_Fortran_COMPILER}
                                      -DCMAKE_Fortran_FLAGS:STRING=${Trilinos_CMAKE_Fortran_FLAGS}
                                      -DCMAKE_INSTALL_PREFIX:PATH=${Trilinos_install_dir}
                                      -DCMAKE_INSTALL_RPATH:PATH=${Trilinos_install_dir}/lib
                                      -DCMAKE_INSTALL_NAME_DIR:PATH=${Trilinos_install_dir}/lib
                                      -DCMAKE_BUILD_TYPE:STRING=${Trilinos_BUILD_TYPE}

                    # -- Build
                    BINARY_DIR       ${Trilinos_build_dir}        # Build directory 
                    BUILD_COMMAND    $(MAKE)                      # $(MAKE) enables parallel builds through make
                    BUILD_IN_SOURCE  ${Trilinos_BUILD_IN_SOURCE}  # Flag for in source builds
                    # -- Install
                    INSTALL_DIR      ${Trilinos_install_dir}      # Install directory
                    # -- Output control
                    ${Trilinos_logging_args}
		    )

# --- Useful variables for packages that depends on Trilinos
global_set(Trilinos_INSTALL_PREFIX "${Trilinos_install_dir}")
global_set(Zoltan_INSTALL_PREFIX "${Trilinos_install_dir}")
