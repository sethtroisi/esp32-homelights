
idf_build_get_property(project_dir PROJECT_DIR)
idf_component_register(
    SRCS "ota.c" "main.cpp"
    EMBED_TXTFILES ${project_dir}/server_certs/lets-encrypt-r3.pem
)
