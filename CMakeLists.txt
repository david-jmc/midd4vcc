cmake_minimum_required(VERSION 3.10)
project(midd4vc C)

set(CMAKE_C_STANDARD 99)

add_executable(Midd4VCServer
    middleware/services/midd4vc_server.c
    middleware/distribution/midd4vc_client.c
    middleware/services/midd4vc_scheduler.c
    middleware/infrastructure/mqtt_adapter.c
    middleware/distribution/midd4vc_job_codec.c
    middleware/specifics/job_catalog.c
)


target_include_directories(Midd4VCServer PRIVATE
    middleware
    middleware/infrastructure
)

target_link_libraries(Midd4VCServer paho-mqtt3c)
target_link_libraries(Midd4VCServer m)
target_link_libraries(Midd4VCServer rt)
