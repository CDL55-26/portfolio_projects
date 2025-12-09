file(GLOB CONTROLLER_SOURCES "controllers/*.c")
file(GLOB SMF_SOURCES        "smf/*.c")
file(GLOB TRANSMITTER_SOURCES        "transmitter/*.c")
idf_component_register(
    SRCS
        "main.c"
        ${CONTROLLER_SOURCES}
        ${SMF_SOURCES}
        ${TRANSMITTER_SOURCES}
    INCLUDE_DIRS
        "."
        "controllers"
        "smf"
        "transmitter"
)
