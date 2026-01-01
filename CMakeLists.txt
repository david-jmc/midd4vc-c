cmake_minimum_required(VERSION 3.10)
project(midd4vc C)

set(CMAKE_C_STANDARD 99)

add_executable(client_app
    apps/client/client_app.c
    middleware/midd4vc_client.c
    platform/posix/mqtt_adapter.c
    middleware/midd4vc_job_codec.c
)

add_executable(vehicle_app
    apps/vehicles/vehicle_app.c
    apps/vehicles/job_catalog.c
    middleware/midd4vc_client.c
    platform/posix/mqtt_adapter.c
    middleware/midd4vc_job_codec.c
)

add_executable(server_app
    apps/server/server_app.c
    middleware/midd4vc_client.c
    platform/posix/mqtt_adapter.c
    middleware/midd4vc_job_codec.c
)

target_include_directories(vehicle_app PRIVATE
    middleware
    platform/posix
)

target_include_directories(server_app PRIVATE
    middleware
    platform/posix
)

target_link_libraries(vehicle_app paho-mqtt3c)
target_link_libraries(server_app paho-mqtt3c)
target_link_libraries(client_app paho-mqtt3c)
target_link_libraries(server_app m)
target_link_libraries(server_app rt)
