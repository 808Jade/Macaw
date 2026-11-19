set(ExtensionVersion 1.6.0)
set(DxcCommit fd7e54bcd)

vcpkg_download_distfile(ArchivePath
    URLS "https://github.com/antaalt/shader-validator/releases/download/v${ExtensionVersion}/shader-validator-win32-x64.vsix"
    FILENAME "shader-validator-win32-x64-${ExtensionVersion}.zip"
    SHA512 6f2f3487d04e7d89af676867d346d42b8a20e5e793fff1b5daf13390b6166b9291db920b228962fb8335fb6a400842e1632861846e421798d8ff3a22cafbf8ac
)

vcpkg_download_distfile(DxcLicense
    URLS "https://raw.githubusercontent.com/microsoft/DirectXShaderCompiler/${DxcCommit}/LICENSE.TXT"
    FILENAME "DXC-${DxcCommit}-LICENSE.txt"
    SHA512 9feaa85ca6d42d5a2d6fe773706bbab8241e78390a9d61ea9061c8f0eeb5a3e380ff07c222e02fbf61af7f2b2f6dd31c5fc87247a94dae275dc0a20cdfcc8c9d
)

vcpkg_download_distfile(DxcNotices
    URLS "https://raw.githubusercontent.com/microsoft/DirectXShaderCompiler/${DxcCommit}/ThirdPartyNotices.txt"
    FILENAME "DXC-${DxcCommit}-ThirdPartyNotices.txt"
    SHA512 72bc04d0bb65f8c07cad7afc4f910363cb5dd72b6287f9fefd1b98987fda38fc66e52563ca274f4f656eec6fbf425e43697e0f1730846d335121f8ee9c54fbac
)

vcpkg_extract_source_archive(SourcePath ARCHIVE "${ArchivePath}" NO_REMOVE_ONE_LEVEL)
set(ToolsDirectory "${CURRENT_PACKAGES_DIR}/tools/hlsl-language-server")
set(ServerLicense "${SourcePath}/extension/LICENSE.txt")
file(INSTALL "${SourcePath}/extension/bin/win32-x64/" DESTINATION "${ToolsDirectory}")
file(INSTALL "${ServerLicense}" "${DxcLicense}" "${DxcNotices}" DESTINATION "${ToolsDirectory}")
vcpkg_install_copyright(FILE_LIST "${ServerLicense}" "${DxcLicense}" "${DxcNotices}")
set(VCPKG_POLICY_EMPTY_INCLUDE_FOLDER enabled)
