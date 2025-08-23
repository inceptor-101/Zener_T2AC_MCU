idf_component_register(
    SRCS "main.c" "../components/canLib/canFunction.c" "../components/BLE/bleFunction.c" "../components/NVS/FetchNVSdata.c" "../components/NVS/SaveNVSdata.c" "../components/Services/services.c" "../components/WiFi/WiFi.c"  # required,
    # list the source files of this component
    INCLUDE_DIRS        # optional, add here public include directories
    PRIV_INCLUDE_DIRS   # optional, add here private include directories
    REQUIRES            # optional, list the public requirements (component names)
    PRIV_REQUIRES       # optional, list the private requirements
)
