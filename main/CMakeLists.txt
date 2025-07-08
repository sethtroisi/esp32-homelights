
idf_build_get_property(project_dir PROJECT_DIR)

idf_component_register(
    SRCS "main.cpp" "HomeLights.cpp"
        "PatternRunner.cpp" "Particle.cpp" "globals.cpp" "fake_shader.cpp"
        "tweaks.cpp"

    REQUIRES driver FastLED
)
