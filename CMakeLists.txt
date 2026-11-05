cmake_minimum_required(VERSION 3.21)

project(Kestrel VERSION 0.1.3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 6.6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Concurrent DBus Test)

include(GNUInstallDirs)

qt_standard_project_setup()

qt_add_library(kestrel_core STATIC
    src/autostartmanager.cpp
    src/autostartmanager.h
    src/calculator.cpp
    src/calculator.h
    src/desktopentry.cpp
    src/desktopentry.h
    src/appindex.cpp
    src/appindex.h
    src/ranker.cpp
    src/ranker.h
    src/usagestore.cpp
    src/usagestore.h
    src/searchmodel.cpp
    src/searchmodel.h
    src/launcher.cpp
    src/launcher.h
    src/krunnerclient.cpp
    src/krunnerclient.h
    src/kglobalaccelclient.cpp
    src/kglobalaccelclient.h
)

target_include_directories(kestrel_core PUBLIC src)
target_link_libraries(kestrel_core PUBLIC Qt6::Core Qt6::Gui Qt6::Qml Qt6::Concurrent Qt6::DBus)

# QML files must live at the module root so `loadFromModule("Kestrel", "Main")` resolves.
# Aliases have to be set before qt_add_qml_module() reads the sources.
set_source_files_properties(qml/Main.qml PROPERTIES QT_RESOURCE_ALIAS Main.qml)
set_source_files_properties(qml/ResultDelegate.qml PROPERTIES QT_RESOURCE_ALIAS ResultDelegate.qml)
set_source_files_properties(qml/ShortcutHint.qml PROPERTIES QT_RESOURCE_ALIAS ShortcutHint.qml)
set_source_files_properties(qml/ActionPanel.qml PROPERTIES QT_RESOURCE_ALIAS ActionPanel.qml)
set_source_files_properties(qml/Toast.qml PROPERTIES QT_RESOURCE_ALIAS Toast.qml)

qt_add_qml_module(kestrel_core
    URI Kestrel
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    QML_FILES
        qml/Main.qml
        qml/ResultDelegate.qml
        qml/ShortcutHint.qml
        qml/ActionPanel.qml
        qml/Toast.qml
)

qt_add_executable(kestrel
    src/main.cpp
    src/iconimageprovider.cpp
    src/iconimageprovider.h
)

target_link_libraries(kestrel PRIVATE kestrel_core kestrel_coreplugin Qt6::Quick Qt6::QuickControls2
                                        Qt6::Gui)
qt_import_qml_plugins(kestrel)

install(TARGETS kestrel RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES packaging/dev.kestrel.Kestrel.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES packaging/dev.kestrel.Kestrel.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)

enable_testing()

qt_add_executable(tst_core tests/tst_core.cpp)
target_link_libraries(tst_core PRIVATE kestrel_core Qt6::Test)
add_test(NAME tst_core COMMAND tst_core)
