# Trains the profile while configuring, so that its path can carry its contents: a new profile
# changes every compile command and so rebuilds everything, which it must, as LTO can't link
# objects compiled with different profiles. An instrumented HanviBench is built in pgo/ and
# converts train/ once. It retrains when the texts or dict.db change; delete pgo/ to retrain
# after changing the code.
set(_pgo_dir "${CMAKE_BINARY_DIR}/pgo")
set(_pgo_inputs "${PROJECT_SOURCE_DIR}/dict.db" ${_training_texts})
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/dict.db")

set(_signature "")
foreach (_input IN LISTS _pgo_inputs)
    file(SIZE "${_input}" _size)
    file(TIMESTAMP "${_input}" _time "%s")
    string(APPEND _signature "${_input} ${_size} ${_time}\n")
endforeach ()
set(_trained_on "")
if (EXISTS "${_pgo_dir}/hanvi.profdata" AND EXISTS "${_pgo_dir}/trained-on.txt")
    file(READ "${_pgo_dir}/trained-on.txt" _trained_on)
endif ()

if (NOT _trained_on STREQUAL _signature)
    list(LENGTH _training_texts _count)
    message(STATUS "PGO: training on ${_count} text(s) in train/; log in ${_pgo_dir}/training.log")
    file(REMOVE "${_pgo_dir}/hanvi.profdata" "${_pgo_dir}/trained-on.txt")
    cmake_host_system_information(RESULT _jobs QUERY NUMBER_OF_LOGICAL_CORES)
    execute_process(
            COMMAND ${CMAKE_COMMAND} -S "${PROJECT_SOURCE_DIR}" -B "${_pgo_dir}" -G "${CMAKE_GENERATOR}"
                    -DCMAKE_BUILD_TYPE=Release
                    "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
                    "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
                    "-DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
                    "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
                    "-DHANVI_IPO=${HANVI_IPO}"
                    "-DHANVI_PGO_TRAINING=${PROJECT_SOURCE_DIR}/train"
            OUTPUT_FILE "${_pgo_dir}/training.log"
            ERROR_FILE "${_pgo_dir}/training.log"
            RESULT_VARIABLE _result)
    if (_result EQUAL 0)
        execute_process(
                COMMAND ${CMAKE_COMMAND} --build "${_pgo_dir}" --target PgoProfile --parallel ${_jobs}
                OUTPUT_FILE "${_pgo_dir}/training.log"
                ERROR_FILE "${_pgo_dir}/training.log"
                RESULT_VARIABLE _result)
    endif ()
    if (NOT _result EQUAL 0)
        message(FATAL_ERROR "PGO training failed, see ${_pgo_dir}/training.log. "
                "Configure with -DHANVI_PGO=OFF to build without it.")
    endif ()
    file(WRITE "${_pgo_dir}/trained-on.txt" "${_signature}")
endif ()

file(SHA256 "${_pgo_dir}/hanvi.profdata" _hash)
string(SUBSTRING "${_hash}" 0 16 _hash)
set(_pgo_profile "${_pgo_dir}/hanvi-${_hash}.profdata")
if (NOT EXISTS "${_pgo_profile}")
    file(GLOB _old_profiles "${_pgo_dir}/hanvi-*.profdata")
    file(REMOVE ${_old_profiles})
    file(COPY_FILE "${_pgo_dir}/hanvi.profdata" "${_pgo_profile}")
endif ()
