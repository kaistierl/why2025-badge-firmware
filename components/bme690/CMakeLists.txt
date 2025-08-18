idf_component_register(
    SRC_DIRS "." "./BME690_SensorAPI/"
    INCLUDE_DIRS "." "./BME690_SensorAPI/"
    REQUIRES "driver"
)

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
