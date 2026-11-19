vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SourcePath
    REPO goossens/ImGuiColorTextEdit
    REF ba4bafe4287a00b8d011ff493d30143a9036f11f
    SHA512 1f77b0c782bbb1d75c0a62d833acba801e59de31cb7c772920c878816647f39b91f40c5a9ab119455c1bb7c2647a8243e5fe820910c588e1e476a2aa335b0ba7
)

file(COPY "${CMAKE_CURRENT_LIST_DIR}/CMakeLists.txt" DESTINATION "${SourcePath}")

vcpkg_cmake_configure(SOURCE_PATH "${SourcePath}")
vcpkg_cmake_install()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

vcpkg_install_copyright(FILE_LIST "${SourcePath}/LICENSE")
