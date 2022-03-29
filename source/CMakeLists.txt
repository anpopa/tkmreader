set(BINARY_NAME tmkreader)
set(BINARY_SRC 
    ${CMAKE_SOURCE_DIR}/shared/Helpers.cpp
    ${CMAKE_SOURCE_DIR}/shared/EnvelopeReader.cpp
    ${CMAKE_SOURCE_DIR}/shared/EnvelopeWriter.cpp
    Dispatcher.cpp
    Connection.cpp
    Command.cpp
    Application.cpp
    Arguments.cpp
    Main.cpp
    )
add_executable(${BINARY_NAME} ${BINARY_SRC})
target_link_libraries(${BINARY_NAME} BSWInfra TaskMonitorInterfaces)
install(TARGETS ${BINARY_NAME} RUNTIME DESTINATION bin)
