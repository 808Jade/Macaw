vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SourcePath
    REPO soufianekhiat/ImAnim
    REF 8a537521ba41b3dfeec5173f2f89a8dc3d6387c8
    SHA512 13ba39f93f064cc0beab648d725b95d96ea835cf5a47547f30fe5e509cc449b6b7dbfde9dcbb322373ad94642b8a4eb857111ab7a3adab23ea22131afae1fe69
)

file(COPY "${CMAKE_CURRENT_LIST_DIR}/CMakeLists.txt" DESTINATION "${SourcePath}")

vcpkg_cmake_configure(SOURCE_PATH "${SourcePath}")
vcpkg_cmake_install()
vcpkg_copy_pdbs()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

vcpkg_install_copyright(FILE_LIST "${SourcePath}/LICENSE")
