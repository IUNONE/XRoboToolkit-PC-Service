# Native, headless tracking build. No vendored platform binaries or GUI demos.
if(CMAKE_VERSION VERSION_LESS 3.18)
    message(FATAL_ERROR "The native dependency build requires CMake >= 3.18")
endif()
include(GNUInstallDirs)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
if(APPLE)
    set(CMAKE_MACOSX_RPATH ON)
    set(CMAKE_INSTALL_RPATH "@loader_path;@loader_path/../${CMAKE_INSTALL_LIBDIR}")
elseif(UNIX)
    set(CMAKE_INSTALL_RPATH "$ORIGIN;$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
endif()
if(MSVC)
    add_compile_options(/utf-8)
endif()

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Network Core5Compat)
find_package(Protobuf CONFIG QUIET)
if(NOT Protobuf_FOUND)
    find_package(Protobuf REQUIRED)
endif()
find_package(gRPC CONFIG REQUIRED)
find_package(Threads REQUIRED)
message(STATUS "Native tracking build: ${CMAKE_SYSTEM_NAME}/${CMAKE_SYSTEM_PROCESSOR}, Qt ${Qt6_VERSION}, Protobuf ${Protobuf_VERSION}")

# The Linux schema is the existing RPC superset. Keep its names and field IDs.
# Generate with the installed protoc instead of mixing generated 4.25.1 headers
# with a different protobuf runtime.
set(XRT_PROTOCOL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/PXREAService/linux_x86")
set(XRT_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${XRT_GENERATED_DIR}")
get_target_property(_protobuf_include_dirs protobuf::libprotobuf INTERFACE_INCLUDE_DIRECTORIES)
find_path(XRT_PROTOBUF_IMPORT_DIR google/protobuf/empty.proto HINTS ${_protobuf_include_dirs} ${Protobuf_INCLUDE_DIRS} REQUIRED)
add_custom_command(
    OUTPUT "${XRT_GENERATED_DIR}/PXREAService.pb.cc" "${XRT_GENERATED_DIR}/PXREAService.pb.h"
           "${XRT_GENERATED_DIR}/PXREAService.grpc.pb.cc" "${XRT_GENERATED_DIR}/PXREAService.grpc.pb.h"
    COMMAND protobuf::protoc
    ARGS "--proto_path=${XRT_PROTOCOL_DIR}" "--proto_path=${XRT_PROTOBUF_IMPORT_DIR}"
         "--cpp_out=${XRT_GENERATED_DIR}" "--grpc_out=${XRT_GENERATED_DIR}"
         "--plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>"
         "${XRT_PROTOCOL_DIR}/PXREAService.proto"
    DEPENDS "${XRT_PROTOCOL_DIR}/PXREAService.proto" protobuf::protoc gRPC::grpc_cpp_plugin
    VERBATIM)
add_library(XRTProtocol STATIC
    "${XRT_GENERATED_DIR}/PXREAService.pb.cc" "${XRT_GENERATED_DIR}/PXREAService.grpc.pb.cc")
set_target_properties(XRTProtocol PROPERTIES AUTOMOC OFF)
target_include_directories(XRTProtocol PUBLIC "${XRT_GENERATED_DIR}")
target_link_libraries(XRTProtocol PUBLIC protobuf::libprotobuf gRPC::grpc++)

add_library(CommonUtils SHARED CommonUtils/commonutils.cpp CommonUtils/commonutils.h)
target_compile_definitions(CommonUtils PRIVATE COMMONUTILS_LIBRARY QT_MESSAGELOGCONTEXT)
target_include_directories(CommonUtils PUBLIC CommonUtils)
target_link_libraries(CommonUtils PUBLIC Qt6::Core Qt6::Network PRIVATE Qt6::Core5Compat)

add_library(DeviceConnectionManager SHARED
    DeviceConnectionManager/DeviceConnectionManager.cpp DeviceConnectionManager/DeviceConnectionManager.h
    DeviceConnectionManager/EventHandler/connecteventhandler.cpp DeviceConnectionManager/EventHandler/connecteventhandler.h
    DeviceConnectionManager/EventHandler/connecteventhandlerinqt.cpp DeviceConnectionManager/EventHandler/connecteventhandlerinqt.h
    DeviceConnectionManager/Manage/connectionmanage.cpp DeviceConnectionManager/Manage/connectionmanage.h
    DeviceConnectionManager/Manage/TCPConnectionManage/tcpconnectionmanage.cpp DeviceConnectionManager/Manage/TCPConnectionManage/tcpconnectionmanage.h
    DeviceConnectionManager/Manage/TCPConnectionManage/tcpserverworker.cpp DeviceConnectionManager/Manage/TCPConnectionManage/tcpserverworker.h
    DeviceConnectionManager/Model/connectionmodel.cpp DeviceConnectionManager/Model/connectionmodel.h
    DeviceConnectionManager/Model/tcpconnectionmodel.cpp DeviceConnectionManager/Model/tcpconnectionmodel.h
    DeviceConnectionManager/utils/utils.cpp)
target_compile_definitions(DeviceConnectionManager PRIVATE DEVICECONNECTIONSDK_LIBRARY)
target_include_directories(DeviceConnectionManager PUBLIC DeviceConnectionManager)
target_link_libraries(DeviceConnectionManager PUBLIC Qt6::Core Qt6::Network CommonUtils)

add_library(Business SHARED
    Business/business.cpp Business/business.h
    Business/DeviceManage/devicemanagement.cpp Business/DeviceManage/devicemanagement.h
    Business/Model/Device/devicemodel.cpp Business/Model/Device/devicemodel.h)
target_compile_definitions(Business PRIVATE BUSINESS_LIBRARY QT_MESSAGELOGCONTEXT)
target_include_directories(Business PUBLIC Business)
target_link_libraries(Business PUBLIC Qt6::Core Qt6::Core5Compat CommonUtils DeviceConnectionManager)

add_library(PXREAGRPCServer SHARED
    PXREAGRPCServer/PXREAGRPCServer.cpp PXREAGRPCServer/PXREAGRPCServer.h
    PXREAGRPCServer/FeedbackController.cpp PXREAGRPCServer/FeedbackController.h)
target_compile_definitions(PXREAGRPCServer PRIVATE PXREASERVERSDK_LIBRARY)
target_include_directories(PXREAGRPCServer PUBLIC PXREAGRPCServer)
target_link_libraries(PXREAGRPCServer PUBLIC Business XRTProtocol PRIVATE gRPC::grpc++_reflection Threads::Threads)

add_library(PXREARobotSDK SHARED PXREARobotSDK/PXREARobotSDK.cpp PXREARobotSDK/PXREARobotSDK.h)
set_target_properties(PXREARobotSDK PROPERTIES AUTOMOC OFF)
target_compile_definitions(PXREARobotSDK PRIVATE PXREACLIENTSDK_LIBRARY)
target_include_directories(PXREARobotSDK PUBLIC PXREARobotSDK)
target_link_libraries(PXREARobotSDK PRIVATE XRTProtocol Threads::Threads)

add_executable(RoboticsServiceProcess RoboticsServiceProcess/main.cpp
    RoboticsServiceProcess/singleapplication.cpp RoboticsServiceProcess/singleapplication.h)
target_link_libraries(RoboticsServiceProcess PRIVATE PXREAGRPCServer Qt6::Core5Compat)

option(XRT_BUILD_SDK_DEMO "Build the headless SDK console example" ON)
if(XRT_BUILD_SDK_DEMO)
    add_executable(ConsoleDemo SDKDemo/CppSrc/ConsoleDemo/main.cpp)
    set_target_properties(ConsoleDemo PROPERTIES AUTOMOC OFF)
    target_link_libraries(ConsoleDemo PRIVATE PXREARobotSDK Threads::Threads)
    install(TARGETS ConsoleDemo RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

configure_file(cmake/native-setting.ini "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/setting.ini" COPYONLY)
install(TARGETS RoboticsServiceProcess CommonUtils DeviceConnectionManager Business PXREAGRPCServer PXREARobotSDK
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES PXREARobotSDK/PXREARobotSDK.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES cmake/native-setting.ini DESTINATION ${CMAKE_INSTALL_BINDIR} RENAME setting.ini)

include(CTest)
if(BUILD_TESTING)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(NAME tracking_service_sdk_roundtrip
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/tracking_service_sdk_roundtrip.py"
                --service "$<TARGET_FILE:RoboticsServiceProcess>" --sdk "$<TARGET_FILE:PXREARobotSDK>")
    set_tests_properties(tracking_service_sdk_roundtrip PROPERTIES TIMEOUT 40)
endif()
