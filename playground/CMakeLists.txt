add_executable(emailkit_playground emailkit_playground_main.cpp)
target_link_libraries(emailkit_playground PRIVATE emailkit::emailkit emailkit::http_srv pthread folly::uri)