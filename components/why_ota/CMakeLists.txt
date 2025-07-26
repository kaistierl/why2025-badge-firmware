set(SRCS "ota.c")

set(DEPS "")

set(INCLUDES "includes")

set(PRIV_DEPS "app_update")

idf_component_register(SRCS ${SRCS}
                       INCLUDE_DIRS ${INCLUDES}
                       PRIV_REQUIRES ${PRIV_DEPS}
                       REQUIRES ${DEPS})