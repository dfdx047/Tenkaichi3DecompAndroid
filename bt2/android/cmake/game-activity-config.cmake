# Stand-in for the Android Game Development Kit's game-activity package, which Granite's Android application layer
# (application/platforms) asks for. That layer is never built here (paraLLEl-GS uses only Granite's Vulkan
# side; SDL is the platform), so the package only needs to exist for CMake to configure.
if(NOT TARGET game-activity::game-activity_static)
    add_library(game-activity::game-activity_static INTERFACE IMPORTED)
endif()
