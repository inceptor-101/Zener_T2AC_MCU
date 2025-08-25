idf_component_register(
    SRCS "main.c" 
         "../components/canLib/canFunction.c" 
         "../components/BLE/bleFunction.c" 
         "../components/NVS/FetchNVSdata.c" 
         "../components/NVS/SaveNVSdata.c" 
         "../components/Services/services.c" 
         "../components/WiFi/wifi_utils.c"  
         "../components/WiFi/wifi_task.c"
         "../components/WiFi/wifi_init.c"
         "../components/WiFi/wifi_conn_scan.c"
         "../components/WiFi/comm_with_BLE.c"
    # required,
    # list the source files of this component
    INCLUDE_DIRS        # optional, add here public include directories
    PRIV_INCLUDE_DIRS   # optional, add here private include directories
    REQUIRES            # optional, list the public requirements (component names)
    PRIV_REQUIRES       # optional, list the private requirements
)
