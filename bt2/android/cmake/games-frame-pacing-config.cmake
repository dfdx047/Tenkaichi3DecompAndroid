# Stand-in for the Android Game Development Kit's games-frame-pacing package, which Granite's Android application layer
# (application/platforms) asks for. That layer is never built here (paraLLEl-GS uses only Granite's Vulkan
# side; SDL is the platform), so the package only needs to exist for CMake to configure.
if(NOT TARGET games-frame-pacing::swappy)
    add_library(games-frame-pacing::swappy INTERFACE IMPORTED)
endif()
