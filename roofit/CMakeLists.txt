add_subdirectory(roofitcore)
add_subdirectory(roofit)
add_subdirectory(roostats)
if(xml OR ROOT_xml_FOUND)
  add_subdirectory(histfactory)
endif()
