# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles/smart_home_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/smart_home_autogen.dir/ParseCache.txt"
  "smart_home_autogen"
  )
endif()
