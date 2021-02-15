file(TO_NATIVE_PATH "${GIT_WORKING_DIRECTORY}" GIT_WORKING_DIRECTORY)
message(STATUS "Cloning '${GIT_URL}' into '${GIT_CLONE_DIR}' at '${GIT_WORKING_DIRECTORY}'")

if (NOT EXISTS "${GIT_WORKING_DIRECTORY}/${GIT_CLONE_DIR}/.git/config")
  execute_process(
    COMMAND git clone ${GIT_URL} ${GIT_CLONE_DIR}
    WORKING_DIRECTORY ${GIT_WORKING_DIRECTORY}
  )
endif()

execute_process(
  COMMAND git reset --hard ${GIT_VERSION}
  WORKING_DIRECTORY ${GIT_WORKING_DIRECTORY}/${GIT_CLONE_DIR}
)
