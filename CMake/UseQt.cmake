
################################################################################
# Qt - https://doc.qt.io/qt-6/cmake-get-started.html
################################################################################

# reads FindQtImport.cmake from the external folder defining the Qt path
find_package(QtImport REQUIRED)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/ui")
add_compile_definitions(QT_DISABLE_DEPRECATED_UP_TO=0x060000)

if (MSVC AND NOT TARGET Qt_CopyPlugins)
    message(VERBOSE "Define Qt copy files targets... (Qt base folder=${QT_BASE_DIR})")
    #[[
        We need to copy plugin and resources files into the output folder. 
        Solution inspired by https://gist.github.com/socantre/7ee63133a0a3a08f3990

        Defines two custom commands generating the files listed below.
        Defines two custom targets depending on these additional files. With that, copying only happens if a file is missing.
        For each executable: And adding this target as a dependency to the imported target above.
    ]]
    set(QT_PLUGINS
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins/iconengines/qsvgicon$<$<CONFIG:Debug>:d>.dll
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins/imageformats/qgif$<$<CONFIG:Debug>:d>.dll
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins/imageformats/qico$<$<CONFIG:Debug>:d>.dll
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins/imageformats/qjpeg$<$<CONFIG:Debug>:d>.dll
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins/imageformats/qsvg$<$<CONFIG:Debug>:d>.dll
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins/imageformats/qtga$<$<CONFIG:Debug>:d>.dll
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins/imageformats/qtiff$<$<CONFIG:Debug>:d>.dll
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins/imageformats/qwbmp$<$<CONFIG:Debug>:d>.dll
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins/imageformats/qwebp$<$<CONFIG:Debug>:d>.dll
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins/multimedia/ffmpegmediaplugin$<$<CONFIG:Debug>:d>.dll
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins/multimedia/windowsmediaplugin$<$<CONFIG:Debug>:d>.dll
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins/platforms/qwindows$<$<CONFIG:Debug>:d>.dll
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins/styles/qmodernwindowsstyle$<$<CONFIG:Debug>:d>.dll
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins/tls/qcertonlybackend$<$<CONFIG:Debug>:d>.dll
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins/tls/qopensslbackend$<$<CONFIG:Debug>:d>.dll
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins/tls/qschannelbackend$<$<CONFIG:Debug>:d>.dll
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/d3dcompiler_47.dll
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/opengl32sw.dll
    )
    add_custom_command(OUTPUT ${QT_PLUGINS}
        COMMAND ${CMAKE_COMMAND} -E copy ${QT_BASE_DIR}/plugins/iconengines/qsvgicon$<$<CONFIG:Debug>:d>.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins/iconengines"
        COMMAND ${CMAKE_COMMAND} -E copy ${QT_BASE_DIR}/plugins/imageformats/qgif$<$<CONFIG:Debug>:d>.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins/imageformats"
        COMMAND ${CMAKE_COMMAND} -E copy ${QT_BASE_DIR}/plugins/imageformats/qicns$<$<CONFIG:Debug>:d>.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins/imageformats"
        COMMAND ${CMAKE_COMMAND} -E copy ${QT_BASE_DIR}/plugins/imageformats/qico$<$<CONFIG:Debug>:d>.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins/imageformats"
        COMMAND ${CMAKE_COMMAND} -E copy ${QT_BASE_DIR}/plugins/imageformats/qjpeg$<$<CONFIG:Debug>:d>.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins/imageformats"
        COMMAND ${CMAKE_COMMAND} -E copy ${QT_BASE_DIR}/plugins/imageformats/qsvg$<$<CONFIG:Debug>:d>.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins/imageformats"
        COMMAND ${CMAKE_COMMAND} -E copy ${QT_BASE_DIR}/plugins/imageformats/qtga$<$<CONFIG:Debug>:d>.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins/imageformats"
        COMMAND ${CMAKE_COMMAND} -E copy ${QT_BASE_DIR}/plugins/imageformats/qtiff$<$<CONFIG:Debug>:d>.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins/imageformats"
        COMMAND ${CMAKE_COMMAND} -E copy ${QT_BASE_DIR}/plugins/imageformats/qwbmp$<$<CONFIG:Debug>:d>.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins/imageformats"
        COMMAND ${CMAKE_COMMAND} -E copy ${QT_BASE_DIR}/plugins/imageformats/qwebp$<$<CONFIG:Debug>:d>.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins/imageformats"
        COMMAND ${CMAKE_COMMAND} -E copy ${QT_BASE_DIR}/plugins/multimedia/ffmpegmediaplugin$<$<CONFIG:Debug>:d>.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins/multimedia"
        COMMAND ${CMAKE_COMMAND} -E copy ${QT_BASE_DIR}/plugins/multimedia/windowsmediaplugin$<$<CONFIG:Debug>:d>.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins/multimedia"
        COMMAND ${CMAKE_COMMAND} -E copy ${QT_BASE_DIR}/plugins/platforms/qwindows$<$<CONFIG:Debug>:d>.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy ${QT_BASE_DIR}/plugins/styles/qmodernwindowsstyle$<$<CONFIG:Debug>:d>.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins/styles"
        COMMAND ${CMAKE_COMMAND} -E copy ${QT_BASE_DIR}/plugins/tls/qcertonlybackend$<$<CONFIG:Debug>:d>.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins/tls"
        COMMAND ${CMAKE_COMMAND} -E copy ${QT_BASE_DIR}/plugins/tls/qopensslbackend$<$<CONFIG:Debug>:d>.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins/tls"
        COMMAND ${CMAKE_COMMAND} -E copy ${QT_BASE_DIR}/plugins/tls/qschannelbackend$<$<CONFIG:Debug>:d>.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins/tls"
        COMMAND ${CMAKE_COMMAND} -E copy ${QT_BASE_DIR}/bin/d3dcompiler_47.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
        COMMAND ${CMAKE_COMMAND} -E copy ${QT_BASE_DIR}/bin/opengl32sw.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
        COMMENT "Copying Qt plugins..."
        VERBATIM
    )
    set(QT_PLUGINS_DEBUG
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins/mediaservice/dsengined.dll
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins/mediaservice/wmfengined.dll
    )
    add_custom_command(OUTPUT ${QT_PLUGINS_DEBUG}
        COMMAND ${CMAKE_COMMAND} -E copy ${QT_BASE_DIR}/plugins/mediaservice/dsengined.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins/mediaservice"
        COMMAND ${CMAKE_COMMAND} -E copy ${QT_BASE_DIR}/plugins/mediaservice/wmfengined.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins/mediaservice"
        COMMENT "Copying Qt plugins for Debug only..."
        VERBATIM
        CONFIG Debug
    )
    add_custom_target(Qt_CopyPlugins DEPENDS ${QT_PLUGINS})
    set_target_properties(Qt_CopyPlugins PROPERTIES FOLDER "Utilities")

endif()
