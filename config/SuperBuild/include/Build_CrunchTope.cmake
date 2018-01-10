#  -*- mode: cmake -*-

#
# Build TPL:  CRUNCHTOPE
#   
# --- Define all the directories and common external project flags
define_external_project_args(CRUNCHTOPE
                             TARGET crunchtope)

list(APPEND CRUNCHTOPE_PACKAGE_DEPENDS ${PETSc_BUILD_TARGET})

# Add version version to the autogenerated tpl_versions.h file
include(${SuperBuild_SOURCE_DIR}/TPLVersions.cmake)
amanzi_tpl_version_write(FILENAME ${TPL_VERSIONS_INCLUDE_FILE}
  PREFIX CRUNCHTOPE
  VERSION ${CRUNCHTOPE_VERSION_MAJOR} ${CRUNCHTOPE_VERSION_MINOR} ${CRUNCHTOPE_VERSION_PATCH})
  

# --- Patch the original code
set(CRUNCHTOPE_patch_file crunchtope-cmake.patch)
set(CRUNCHTOPE_sh_patch ${CRUNCHTOPE_prefix_dir}/crunchtope-patch-step.sh)
configure_file(${SuperBuild_TEMPLATE_FILES_DIR}/crunchtope-patch-step.sh.in
               ${CRUNCHTOPE_sh_patch}
               @ONLY)
# configure the CMake patch step
set(CRUNCHTOPE_cmake_patch ${CRUNCHTOPE_prefix_dir}/crunchtope-patch-step.cmake)
configure_file(${SuperBuild_TEMPLATE_FILES_DIR}/crunchtope-patch-step.cmake.in
               ${CRUNCHTOPE_cmake_patch}
               @ONLY)
# set the patch command
set(CRUNCHTOPE_PATCH_COMMAND ${CMAKE_COMMAND} -P ${CRUNCHTOPE_cmake_patch})

# --- Define the arguments passed to CMake.
set(CRUNCHTOPE_CMAKE_ARGS 
      "-DCMAKE_INSTALL_PREFIX:FILEPATH=${TPL_INSTALL_PREFIX}"
      "-DBUILD_SHARED_LIBS:BOOL=${BUILD_SHARED_LIBS}"
      "-DCMAKE_Fortran_FLAGS:STRING=-w -DALQUIMIA -Wall -fPIC -Wno-unused-variable -ffree-line-length-0 -O3"
      "-DPETSC_DIR=${PETSC_DIR}"
      "-DPETSC_ARCH=.")

# --- Add external project build and tie to the CRUNCHTOPE build target
ExternalProject_Add(${CRUNCHTOPE_BUILD_TARGET}
                    DEPENDS   ${CRUNCHTOPE_PACKAGE_DEPENDS}           # Package dependency target
                    TMP_DIR   ${CRUNCHTOPE_tmp_dir}                   # Temporary files directory
                    STAMP_DIR ${CRUNCHTOPE_stamp_dir}                 # Timestamp and log directory
                    # -- Download and URL definitions
                    DOWNLOAD_DIR ${TPL_DOWNLOAD_DIR}                  # Download directory
                    URL          ${CRUNCHTOPE_URL}                    # URL may be a web site OR a local file
                    URL_MD5      ${CRUNCHTOPE_MD5_SUM}                # md5sum of the archive file
                    # -- Patch 
                    PATCH_COMMAND ${CRUNCHTOPE_PATCH_COMMAND}         # Mods to source
                    # -- Configure
                    SOURCE_DIR    ${CRUNCHTOPE_source_dir}            # Source directory
                    CMAKE_ARGS    ${CRUNCHTOPE_CMAKE_ARGS}            # CMAKE_CACHE_ARGS or CMAKE_ARGS => CMake configure
                                  -DCMAKE_C_FLAGS:STRING=${Amanzi_COMMON_CFLAGS}  # Ensure uniform build
                                  -DCMAKE_C_COMPILER:FILEPATH=${CMAKE_C_COMPILER}
                                  -DCMAKE_Fortran_COMPILER:FILEPATH=${CMAKE_Fortran_COMPILER}

                    # -- Build
                    BINARY_DIR      ${CRUNCHTOPE_build_dir}           # Build directory 
                    BUILD_COMMAND   $(MAKE)
                    # -- Install
                    INSTALL_DIR     ${TPL_INSTALL_PREFIX}             # Install directory
                    INSTALL_COMMAND $(MAKE) install
                    # -- Output control
                    ${CRUNCHTOPE_logging_args})

include(BuildLibraryName)
build_library_name(crunchchem CRUNCHTOPE_LIB APPEND_PATH ${TPL_INSTALL_PREFIX}/lib)
global_set(CRUNCHTOPE_INCLUDE_DIRS ${TPL_INSTALL_PREFIX})
global_set(CRUNCHTOPE_DIR ${TPL_INSTALL_PREFIX})
