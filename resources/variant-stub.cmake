# Syntax:  cmake -P variant-stub.cmake <input> <output>
# Applies post-cleanup of generated Mitsuba Python stubs

file(READ ${CMAKE_ARGV3} FILE_CONTENTS)
string(REPLACE "drjit.llvm" "drjit.auto" FILE_CONTENTS "${FILE_CONTENTS}")
string(REPLACE "drjit.cuda" "drjit.auto" FILE_CONTENTS "${FILE_CONTENTS}")
string(REPLACE "types.CapsuleType" "object" FILE_CONTENTS "${FILE_CONTENTS}")
string(REPLACE "filesystem.path" "str" FILE_CONTENTS "${FILE_CONTENTS}")
string(REPLACE "scalar_rgb." "" FILE_CONTENTS "${FILE_CONTENTS}")
string(REPLACE "class Properties(_Properties)" "class Properties" FILE_CONTENTS "${FILE_CONTENTS}")
string(REPLACE "_Properties" "Properties" FILE_CONTENTS "${FILE_CONTENTS}")
string(REGEX REPLACE "[\n ]+python\\.[^-\\s]+ as python\\.[^-\\s]+," "" FILE_CONTENTS "${FILE_CONTENTS}")
string(REGEX REPLACE "import mitsuba as [a-z_]+" "" FILE_CONTENTS "${FILE_CONTENTS}")

file(WRITE "${CMAKE_ARGV4}" "${FILE_CONTENTS}")