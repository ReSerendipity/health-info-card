# Normalize $ENV{IDF_PATH} to forward slashes: on Windows the native value
# uses backslashes (e.g. C:\Users\...), and a sequence like "\U" in
# "C:\Users" would be parsed as an invalid escape inside a quoted string.
file(TO_CMAKE_PATH "$ENV{IDF_PATH}/components/bootloader_support/private_include" IDF_BOOTLOADER_PRIV_INCLUDE)

idf_component_register(
    SRCS "recovery_boot_hook.c"
    PRIV_REQUIRES bootloader_support
    PRIV_INCLUDE_DIRS "${IDF_BOOTLOADER_PRIV_INCLUDE}"
)
