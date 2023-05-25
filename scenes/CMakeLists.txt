install(DIRECTORY . DESTINATION scenes COMPONENT scenes
    PATTERN ignis_cache* EXCLUDE
    PATTERN blender EXCLUDE
    PATTERN mitsuba EXCLUDE
    PATTERN *.blend EXCLUDE
    PATTERN *.sh EXCLUDE
)