set(BINARY_NAME tkm-reader)
set(BINARY_SRC 
    Query.cpp
    JsonWriter.cpp
    Dispatcher.cpp
    Connection.cpp
    Application.cpp
    Arguments.cpp
    SQLiteDatabase.cpp
    Main.cpp
    )
add_executable(${BINARY_NAME} ${BINARY_SRC})
target_link_libraries(${BINARY_NAME} ${LIBS} BSWInfra)
install(TARGETS ${BINARY_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
