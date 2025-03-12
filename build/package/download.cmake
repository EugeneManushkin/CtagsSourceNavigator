message("Start downloading '${URL}' to file '${FILENAME}'")
file(DOWNLOAD "${URL}" "${FILENAME}" STATUS RESULT)
list(GET RESULT 0 RESULT_CODE)
if (NOT RESULT_CODE EQUAL 0)
    message(FATAL_ERROR "Failed to download '${URL}' to '${FILENAME}' with error: ${RESULT}")
endif()
message("Downloaded ${FILENAME}.")
