set(ReleaseVersion 3.19.2)
set(ArchiveName "lua-language-server-${ReleaseVersion}-win32-x64.zip")

vcpkg_download_distfile(ArchivePath
    URLS "https://github.com/LuaLS/lua-language-server/releases/download/${ReleaseVersion}/${ArchiveName}"
    FILENAME "${ArchiveName}"
    SHA512 ac50e0037248f83264382d6e0f245b59410120e19e7c42c09dfb6d12789d5a57a8bd1f69d7ee170d91b2fc6d9933128cd016bb32a7306bdca1c0601c5279485d
)

vcpkg_extract_source_archive(SourcePath ARCHIVE "${ArchivePath}" NO_REMOVE_ONE_LEVEL)
file(INSTALL "${SourcePath}/" DESTINATION "${CURRENT_PACKAGES_DIR}/tools/lua-language-server")
vcpkg_install_copyright(FILE_LIST "${SourcePath}/LICENSE")
set(VCPKG_POLICY_EMPTY_INCLUDE_FOLDER enabled)
