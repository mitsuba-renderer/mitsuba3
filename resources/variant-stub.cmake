# Syntax:  cmake -P variant-stub.cmake <input> <output>
# Applies some post-cleanup on generated Mitsuba Python stubs

file(READ ${CMAKE_ARGV3} FILE_CONTENTS)
string(REPLACE "drjit.llvm" "drjit.auto" FILE_CONTENTS "${FILE_CONTENTS}")
string(REPLACE "drjit.cuda" "drjit.auto" FILE_CONTENTS "${FILE_CONTENTS}")
string(REPLACE "types.CapsuleType" "object" FILE_CONTENTS "${FILE_CONTENTS}")

file(WRITE "${CMAKE_ARGV4}" "${FILE_CONTENTS}")

