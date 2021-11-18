
idf_build_get_property(project_dir PROJECT_DIR)

idf_component_register(
    SRCS "main.cpp" "HomeLights.cpp" "Networking.cpp"

#    "ota.cpp"
#    EMBED_TXTFILES ${project_dir}/server_certs/lets-encrypt-r3.pem
)
