# Fix the .pdb install rule for Windows. The rule names the file of a
# single-configuration build -- "raw.pdb" in the build root -- but the
# Visual Studio generator puts it in a per-configuration directory and the
# Debug configuration adds a "d" to the name, so a Debug build fails to
# install. The generator expression resolves to whatever the file is
# actually called. Applied to LibRaw-cmake at the commit BuildLibRaw.cmake
# pins; a different commit may not contain the text, so check after
# bumping the pin.
#
# Usage: cmake -DLIBRAW_CMAKELISTS=<file> -P patch.cmake

file(READ "${LIBRAW_CMAKELISTS}" contents)
string(REPLACE
    [[install(FILES ${PROJECT_BINARY_DIR}/raw.pdb ${PROJECT_BINARY_DIR}/raw_r.pdb]]
    [[install(FILES $<TARGET_PDB_FILE:raw> $<TARGET_PDB_FILE:raw_r>]]
    contents "${contents}")
file(WRITE "${LIBRAW_CMAKELISTS}" "${contents}")
