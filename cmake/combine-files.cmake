message(STATUS "COMBINE_INPUT_FILES: ${COMBINE_INPUT_FILES}")
message(STATUS "COMBINE_OUTPUT_FILE: ${COMBINE_OUTPUT_FILE}")
# Prepare a temporary file to "cat" to:
file(WRITE ${COMBINE_OUTPUT_FILE} "")

function(cat IN_FILE OUT_FILE)
  file(READ ${IN_FILE} CONTENTS)
  file(APPEND ${OUT_FILE} "${CONTENTS}")
endfunction()
# Call the "cat" function for each input file
foreach(file ${COMBINE_INPUT_FILES})
  cat(${file} ${COMBINE_OUTPUT_FILE})
endforeach()
