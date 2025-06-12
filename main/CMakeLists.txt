idf_component_register(SRCS "wifi_sta_handlers.c" "main.c" "Wifi.c" "wifi_sta_handlers.c"
                    INCLUDE_DIRS "."
                    EMBED_FILES captive_portal.html ../webpage/index.html ../webpage/nav.html ../webpage/styles.css ../webpage/comon.js ../webpage/ws.js ../webpage/status.html ../webpage/control.html ../webpage/calibrate.html
                    )
