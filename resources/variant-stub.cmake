# Syntax:  cmake -P variant-stub.cmake <input> <output>
# Applies post-cleanup of generated Mitsuba Python stubs

file(READ ${CMAKE_ARGV3} FILE_CONTENTS)
string(REPLACE "drjit.llvm" "drjit.auto" FILE_CONTENTS "${FILE_CONTENTS}")
string(REPLACE "drjit.cuda" "drjit.auto" FILE_CONTENTS "${FILE_CONTENTS}")
string(REPLACE "types.CapsuleType" "object" FILE_CONTENTS "${FILE_CONTENTS}")
string(REGEX REPLACE "(mitsuba\\.)?filesystem\\.path" "str" FILE_CONTENTS "${FILE_CONTENTS}")
string(REPLACE "scalar_rgb." "" FILE_CONTENTS "${FILE_CONTENTS}")

# Erasing the qualifier above leaves the matching import behind, and no stub
# accompanies the variant module.
string(REGEX REPLACE "import mitsuba\\.scalar_rgb\n" "" FILE_CONTENTS "${FILE_CONTENTS}")

file(WRITE "${CMAKE_ARGV4}" "${FILE_CONTENTS}")
