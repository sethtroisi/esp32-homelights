
idf_build_get_property(project_dir PROJECT_DIR)

idf_component_register(
    SRCS "wifi_sync.cpp" "main.cpp" "HomeLights.cpp"
        "PatternRunner.cpp" "Particle.cpp" "globals.cpp" "fake_shader.cpp"
        "tweaks.cpp"

    REQUIRES driver nvs_flash FastLED
)
