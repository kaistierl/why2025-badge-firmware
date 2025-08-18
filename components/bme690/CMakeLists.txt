idf_component_register(
    SRC_DIRS "." "./sensor_api/"
    INCLUDE_DIRS "." "./sensor_api/"
    REQUIRES "driver"
)

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
