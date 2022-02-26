
idf_build_get_property(project_dir PROJECT_DIR)

idf_component_register(
    SRCS "Networking.cpp" "main.cpp" "HomeLights.cpp"
        "PatternRunner.cpp" "globals.cpp" "fake_shader.cpp"
#    "ota.cpp"
)
