# This file is part of enblend.
# Licence details can be found in the file COPYING.
#
# Copyright (c) 2009, Kornel Benko <Kornel.Benko@berlin.de>
#                   , Ryan Sleevi <ryan+hugin@sleevi.com>
#                   , Harry van der Wolf <hvdwolf@gmail.com>
#

include(CheckIncludeFile)
include(CheckIncludeFileCXX)
include(CheckIncludeFiles)
include(CheckSymbolExists)
include(CheckFunctionExists)
include(CheckLibraryExists)
include(CheckTypeSize)
include(CheckCXXSourceCompiles)
include(CheckFunctionExists)
include(TestBigEndian)

#Check for some include files and set appropriate variables
# e.g. "sys/dir.h" found => "HAVE_SYS_DIR_H" set to 1
foreach(_fl "dirent.h" "ext/slist" "fenv.h"
    "GLUT/glut.h" "GL/glut.h" "GL/glu.h" "GL/gl.h"
    "OpenGL/glu.h" "OpenGL/gl.h"
    "inttypes.h" "limits.h"
    "memory.h" "stdint.h" "stdlib.h" "stdbool.h" "strings.h" "string.h"
    "sys/stat.h" "sys/types.h" "unistd.h" "windows.h")
  string(REGEX REPLACE "[/\\.]" "_" _var ${_fl})
  string(TOUPPER "${_var}" _FLN)
  check_include_file_cxx("${_fl}" "HAVE_${_FLN}" )
endforeach()

#Check for functions
set(CMAKE_REQUIRED_LIBRARIES -lm)
foreach(_fc fesetround floor fseeko lrint lrintf
    memset mkstemp pow rint sqrt malloc
    strchr strcspn strdup strerror strerror_r strrchr strtol)
  string(TOUPPER "${_fc}" _FC)
  check_function_exists(${_fc} "HAVE_${_FC}")
endforeach()

if (${HAVE_DIRENT_H})
  check_cxx_source_compiles(
    "
    #include <dirent.h>
    DIR *DI;
    int i = closedir(DI);
    int main(){return(0);}
    "
    CLOSEDIR_INT)
endif()
if (NOT CLOSEDIR_INT)
  set(CLOSEDIR_VOID 1)
endif()

message(STATUS "CMAKE_ERRFILE = ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeError.log")
message(STATUS "CMAKE_LOGFILE = ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeOutput.log")
if (APPLE)
  message(STATUS "We are on Apple OSX. Detect OpenGL framework")
  find_package(OpenGL)
endif(APPLE)

find_package(GLUT)
find_package(GLEW)
set(HAVE_LIBGLEW ${GLEW_FOUND})
find_library(HAVE_LIBLCMS lcms REQUIRED)
find_library(HAVE_LIBXMI xmi REQUIRED)
find_library(HAVE_LIBZ z)
find_package(Threads)
if(CMAKE_HAVE_PTHREAD_H AND CMAKE_HAVE_PTHREAD_CREATE)
  set(HAVE_PTHREAD 1)
endif()

check_type_size( ptrdiff_t    HAVE_PTRDIFF_T )
find_package(OpenMP)
if(${OpenMP_FLAG_DETECTED})
  add_definitions(${OpenMP_CXX_FLAGS})
  set(HAVE_OPENMP 1)
endif()
# How to check  if stdbool.h conforms to C99?

check_cxx_source_compiles(
    "
    #include <sys/dir.h>
    DIR *a = 0;
    int main(){return(0);}
    "
    HAVE_SYS_DIR_H)
check_cxx_source_compiles(
    "
    #include <sys/ndir.h>
    DIR *a = 0;
    int main(){return(0);}
    "
    HAVE_SYS_NDIR_H)
find_package(OpenEXR REQUIRED)
set(HasEXR ${OPENEXR_FOUND})
find_package(JPEG REQUIRED)
set(HasJPEG ${JPEG_FOUND})
find_package(PNG REQUIRED)
set(HasPNG ${PNG_FOUND})
find_package(TIFF REQUIRED)
set(HasTIFF ${TIFF_FOUND})
check_include_files("stdlib.h;stdarg.h;string.h;float.h" STDC_HEADERS)
check_cxx_source_compiles(
  "
    #include <signal.h>

    static void sg(int b) { return;}
    int main(int c, char *av[]) { signal(0, sg); return(0);}
    "
  RETSIGTYPE_VOID)
if (RETSIGTYPE_VOID)
  set(RETSIGTYPE void)
else()
  set(RETSIGTYPE int)
endif()
test_big_endian(WORDS_BIGENDIAN)
check_cxx_source_compiles(
  "
    #include <sys/types.h>
    off_t a = 0;
    int main(){return(0);}
    "
  HAVE_OFF_T)

check_cxx_source_compiles(
  "
    #include <sys/types.h>
    size_t a = 0;
    int main(){return(0);}
    "
  HAVE_SIZE_T)
check_cxx_source_compiles(
  "
  #include <string.h>
  int main(){char b;char *a = strerror_r(0, &b, 0); return(0);}
  "
  STRERROR_R_CHAR_P)
