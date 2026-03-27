cmake_minimum_required(VERSION 3.10)
project(midd4vc C)

set(CMAKE_C_STANDARD 99)

#add_executable(client_app
#    apps/client/client_app.c
#    middleware/distribution/midd4vc_client.c
#    middleware/infrastructure/mqtt_adapter.c
#    middleware/distribution/midd4vc_job_codec.c
#)

#add_executable(client_exp
#    apps/client/client_exp.c
#    middleware/distribution/midd4vc_client.c
#    middleware/infrastructure/mqtt_adapter.c
#    middleware/distribution/midd4vc_job_codec.c
#)

#add_executable(client
#    apps/client/client.c
#    middleware/distribution/midd4vc_client.c
#    middleware/infrastructure/mqtt_adapter.c
#    middleware/distribution/midd4vc_job_codec.c
#)

#add_executable(vehicle_app
#    apps/vehicles/vehicle_app.c
#    apps/vehicles/job_catalog.c
#    middleware/distribution/midd4vc_client.c
#    middleware/infrastructure/mqtt_adapter.c
#    middleware/distribution/midd4vc_job_codec.c
#)

#add_executable(stationary
#    apps/vehicles/stationary.c
#    apps/vehicles/job_catalog.c
#    middleware/distribution/midd4vc_client.c
#    middleware/infrastructure/mqtt_adapter.c
#    middleware/distribution/midd4vc_job_codec.c
#)

#add_executable(mobile_app
#    apps/vehicles/mobile_app.c
#    apps/vehicles/job_catalog.c
#    middleware/distribution/midd4vc_client.c
#    middleware/infrastructure/mqtt_adapter.c
#    middleware/distribution/midd4vc_job_codec.c
#)

#add_executable(vehicle
#    apps/vehicles/vehicle.c
#    apps/vehicles/job_catalog.c
#    middleware/distribution/midd4vc_client.c
#    middleware/infrastructure/mqtt_adapter.c
#    middleware/distribution/midd4vc_job_codec.c
#)

add_executable(flooding_app
    apps/vehicles/flooding_app.c
    apps/vehicles/job_catalog.c
    middleware/distribution/midd4vc_client.c
    middleware/infrastructure/mqtt_adapter.c
    middleware/distribution/midd4vc_job_codec.c
)

add_executable(Midd4VCServer
    apps/server/server_app.c
    middleware/distribution/midd4vc_client.c
    middleware/services/midd4vc_scheduler.c
    middleware/infrastructure/mqtt_adapter.c
    middleware/distribution/midd4vc_job_codec.c
    middleware/specifics/job_catalog.c
)

#target_include_directories(vehicle_app PRIVATE
#    middleware
#    middleware/services
#    middleware/infrastructure
#)

#target_include_directories(stationary PRIVATE
#    middleware
#    middleware/services
#    middleware/infrastructure
#)

#target_include_directories(mobile_app PRIVATE
#    middleware
#    middleware/services
#    middleware/infrastructure
#)

#target_include_directories(vehicle PRIVATE
#    middleware
#    middleware/services
#    middleware/infrastructure
#)

target_include_directories(flooding_app PRIVATE
    middleware
    middleware/services
    middleware/infrastructure
)

target_include_directories(Midd4VCServer PRIVATE
    middleware
    middleware/infrastructure
)

# Gerar a biblioteca dinâmica (.so)
add_library(math_fib SHARED middleware/specifics/math_fib.c)
# Garantir que o nome seja math_fib.so (sem o prefixo 'lib')
set_target_properties(math_fib PROPERTIES PREFIX "")

#target_link_libraries(vehicle_app paho-mqtt3c)
#target_link_libraries(mobile_app paho-mqtt3c)
#target_link_libraries(stationary paho-mqtt3c)
#target_link_libraries(vehicle paho-mqtt3c)
target_link_libraries(flooding_app paho-mqtt3c)
target_link_libraries(Midd4VCServer paho-mqtt3c)
#target_link_libraries(client_app paho-mqtt3c)
#target_link_libraries(client_exp paho-mqtt3c)
#target_link_libraries(client paho-mqtt3c)
target_link_libraries(Midd4VCServer m)
target_link_libraries(Midd4VCServer rt)
