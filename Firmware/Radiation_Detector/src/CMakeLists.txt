SET(SOURCES ui_InitialScreen.c
    ui_MainScreen.c
    ui_Charts1h.c
    ui_VoltageScreen.c
    ui_Settings.c
    ui_Charts24h.c
    ui_ChartsSpectrum.c
    ui_Startup_screen.c
    ui.c
    ui_comp_hook.c
    ui_helpers.c
    ui_img_176188083.c
    ui_img_star_png.c
    ui_img_radiation_png.c
    ui_img_1345346577.c
    ui_img_electricity_png.c
    ui_img_settings_png.c
    ui_img_584212719.c
    ui_img_radscan200x200_png.c
    fonts/ui_font_Pixel.c
    fonts/ui_font_Pixel_20.c
    fonts/ui_font_Pixel_60.c
    fonts/ui_font_SonicSaber.c)

add_library(ui ${SOURCES})
