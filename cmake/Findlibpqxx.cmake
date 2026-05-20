# - Try to find libpqxx
# Once done this will define
#  libpqxx_FOUND - System has libpqxx
#  libpqxx_INCLUDE_DIRS - The libpqxx include directories
#  libpqxx_LIBRARIES - The libraries needed to use libpqxx

find_path(libpqxx_INCLUDE_DIR NAMES pqxx/pqxx
  PATH_SUFFIXES include
)

find_library(libpqxx_LIBRARY NAMES pqxx
  PATH_SUFFIXES lib lib64
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(libpqxx
  REQUIRED_VARS libpqxx_LIBRARY libpqxx_INCLUDE_DIR
)

if(libpqxx_FOUND)
  set(libpqxx_LIBRARIES ${libpqxx_LIBRARY})
  set(libpqxx_INCLUDE_DIRS ${libpqxx_INCLUDE_DIR})
  
  if(NOT TARGET libpqxx::pqxx)
    add_library(libpqxx::pqxx UNKNOWN IMPORTED)
    set_target_properties(libpqxx::pqxx PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${libpqxx_INCLUDE_DIR}"
      IMPORTED_LINK_INTERFACE_LANGUAGES "CXX"
      IMPORTED_LOCATION "${libpqxx_LIBRARY}"
    )
    
    # Locate libpq because libpqxx links against it
    find_library(libpq_LIBRARY NAMES pq)
    if(libpq_LIBRARY)
      set_target_properties(libpqxx::pqxx PROPERTIES
        INTERFACE_LINK_LIBRARIES "${libpq_LIBRARY}"
      )
    endif()
  endif()
endif()

mark_as_advanced(libpqxx_INCLUDE_DIR libpqxx_LIBRARY)
