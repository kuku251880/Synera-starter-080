# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles\\Synera_Starter_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\Synera_Starter_autogen.dir\\ParseCache.txt"
  "Synera_Starter_autogen"
  )
endif()
