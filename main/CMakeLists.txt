idf_component_register(
    SRCS
        "main.c"
        "hanzi/siwu_hanzi_table.c"
        "hanzi/hanzi_deck.c"
        "hanzi/voice_adpcm.c"
        "hanzi/voice_pack.c"
        "hanzi/audio_worker.c"
        "hanzi/progress_nvs.c"
        "hanzi/settings_nvs.c"
        "ui/kids_ui.c"
        "../assets/fonts/kids_font_56.c"
        "../assets/fonts/kids_font_48.c"
        "../assets/fonts/kids_font_32.c"
        "../assets/fonts/kids_font_20.c"
        "../assets/images/thumb_up.c"
    INCLUDE_DIRS "." "hanzi" "ui"
    REQUIRES bsp nvs_flash
    EMBED_FILES "../assets/voice/hanzi_voice_pack.bin"
)
