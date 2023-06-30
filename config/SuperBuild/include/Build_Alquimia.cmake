#  -*- mode: cmake -*-

#
# Build TPL:  ALQUIMIA 
#   
# --- Define all the directories and common external project flags
define_external_project_args(Alquimia
                             TARGET alquimia
                             DEPENDS PETSc PFLOTRAN CrunchTope)

# add version to the autogenerated tpl_versions.h file
include(${SuperBuild_SOURCE_DIR}/TPLVersions.cmake)
amanzi_tpl_version_write(FILENAME ${TPL_VERSIONS_INCLUDE_FILE}
                         PREFIX Alquimia
                         VERSION ${ALQUIMIA_VERSION_MAJOR} ${ALQUIMIA_VERSION_MINOR} ${ALQUIMIA_VERSION_PATCH})

# --- Patch the original code
# Alquimia and Amanzi disagree about how to find PETSc, so we override 
set(Alquimia_patch_file alquimia-cmake.patch
                        alquimia-FindPETSc.patch
                        alquimia-MPIlocation.patch
                        alquimia-undefined_ierr.patch
                      )
set(Alquimia_sh_patch ${Alquimia_prefix_dir}/alquimia-patch-step.sh)
configure_file(${SuperBuild_TEMPLATE_FILES_DIR}/alquimia-patch-step.sh.in
               ${Alquimia_sh_patch}
               @ONLY)
# configure the CMake patch step
set(Alquimia_cmake_patch ${Alquimia_prefix_dir}/alquimia-patch-step.cmake)
configure_file(${SuperBuild_TEMPLATE_FILES_DIR}/alquimia-patch-step.cmake.in
               ${Alquimia_cmake_patch}
               @ONLY)
# set the patch command
set(Alquimia_PATCH_COMMAND ${CMAKE_COMMAND} -P ${Alquimia_cmake_patch})

# --- Define the arguments passed to CMake.
set(suffix "a")
if (BUILD_SHARED_LIBS)
  if (APPLE)
    set(suffix "dylib")
  else()
    set(suffix "so")
  endif()
endif()

set(Alquimia_CMAKE_ARGS 
      "-DCMAKE_INSTALL_PREFIX:FILEPATH=${TPL_INSTALL_PREFIX}"
      "-DBUILD_SHARED_LIBS:BOOL=${BUILD_SHARED_LIBS}"
      "-DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}"
      "-DPETSC_DIR=${PETSc_DIR}"
      "-DXSDK_WITH_PFLOTRAN:BOOL=${ENABLE_PFLOTRAN}" 
      "-DTPL_PFLOTRAN_LIBRARIES:FILEPATH=${PFLOTRAN_DIR}/lib/libpflotranchem.a" 
      "-DTPL_PFLOTRAN_INCLUDE_DIRS:FILEPATH=${PFLOTRAN_INCLUDE_DIRS}"
      "-DXSDK_WITH_CRUNCHFLOW:BOOL=${ENABLE_CRUNCHTOPE}"
      "-DTPL_CRUNCHFLOW_LIBRARIES:FILEPATH=${CrunchTope_DIR}/lib/libcrunchchem.${suffix}"
      "-DTPL_CRUNCHFLOW_INCLUDE_DIRS:FILEPATH=${TPL_INSTALL_PREFIX}/lib"
      "-DLAPACK_LIBRARIES=${LAPACK_LIBRARIES}"
      "-DMPI_PREFIX=${MPI_PREFIX}"
      "-DCMAKE_Fortran_FLAGS:STRING=-fPIC -w -Wno-unused-variable -ffree-line-length-0 -O3")

# --- Add external project build and tie to the Alquimia build target
ExternalProject_Add(${Alquimia_BUILD_TARGET}
                    DEPENDS   ${Alquimia_PACKAGE_DEPENDS}         # Package dependency target
                    TMP_DIR   ${Alquimia_tmp_dir}                 # Temporary files directory
                    STAMP_DIR ${Alquimia_stamp_dir}               # Timestamp and log directory
                    # -- Download and URL definitions
                    DOWNLOAD_DIR ${TPL_DOWNLOAD_DIR}              # Download directory
                    URL          ${Alquimia_URL}                  # URL may be a web site OR a local file
                    URL_MD5      ${Alquimia_MD5_SUM}              # md5sum of the archive file
                    DOWNLOAD_NAME ${Alquimia_SAVEAS_FILE}         # file name to store (if not end of URL)
                    PATCH_COMMAND ${Alquimia_PATCH_COMMAND}       # Mods to source
                    # -- Configure
                    SOURCE_DIR    ${Alquimia_source_dir}          # Source directory
                    CMAKE_ARGS    ${AMANZI_CMAKE_CACHE_ARGS}      # Ensure uniform build
                                  ${Alquimia_CMAKE_ARGS}
                                  -DCMAKE_C_FLAGS:STRING=${Amanzi_COMMON_CFLAGS}  # Ensure uniform build
                                  -DCMAKE_C_COMPILER:FILEPATH=${CMAKE_C_COMPILER}
                                  -DCMAKE_CXX_FLAGS:STRING=${Amanzi_COMMON_CXXFLAGS}  # Ensure uniform build
                                  -DCMAKE_CXX_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER}
                                  -DCMAKE_Fortran_FLAGS:STRING=${Amanzi_COMMON_FCFLAGS}
                                  -DCMAKE_Fortran_COMPILER:FILEPATH=${CMAKE_Fortran_COMPILER}
                    # -- Build
                    BINARY_DIR        ${Alquimia_build_dir}       # Build directory 
                    BUILD_COMMAND     $(MAKE)
                    # -- Install
                    INSTALL_DIR      ${TPL_INSTALL_PREFIX}        # Install directory
                    INSTALL_COMMAND  $(MAKE) install
                    # -- Output control
                    # -- Output control
                    ${Alquimia_logging_args})

include(BuildLibraryName)
build_library_name(alquimia_c Alquimia_C_LIB APPEND_PATH ${TPL_INSTALL_PREFIX}/lib)
build_library_name(alquimia_cutils Alquimia_CUTILS_LIB APPEND_PATH ${TPL_INSTALL_PREFIX}/lib)
build_library_name(alquimia_fortran Alquimia_F_LIB APPEND_PATH ${TPL_INSTALL_PREFIX}/lib)
set(Alquimia_INSTALL_PREFIX ${TPL_INSTALL_PREFIX})
