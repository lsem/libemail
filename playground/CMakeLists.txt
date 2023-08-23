add_executable(emailkit_playground emailkit_playground_main.cpp)
add_dependencies(emailkit_playground external_glib external_gmime)
target_link_libraries(emailkit_playground
    PRIVATE
        emailkit::emailkit
        emailkit::http_srv
        pthread
        folly::uri
        b64::b64
        gmime::gmime)

target_compile_options(emailkit_playground PRIVATE "-fsanitize=address")
target_link_options(emailkit_playground PRIVATE "-fsanitize=address")
