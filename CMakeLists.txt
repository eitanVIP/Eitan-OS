cmake_minimum_required(VERSION 3.16)
project(eitanos C ASM_NASM)

enable_language(ASM_NASM)

# Collect all C and ASM files under src/
file(GLOB SRC_FILES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/*.c" "${CMAKE_SOURCE_DIR}/src/*.S")

# Object files list
set(OBJ_FILES "")

foreach(SRC ${SRC_FILES})
    get_filename_component(NAME ${SRC} NAME_WE)  # NAME_WE = name without extension
    set(OBJ "${CMAKE_BINARY_DIR}/${NAME}.o")      # output file named e.g. kernel.o, start.o

    if(${SRC} MATCHES ".+\\.S$")
        add_custom_command(
                OUTPUT ${OBJ}
                COMMAND nasm -f elf32 ${SRC} -o ${OBJ}
                DEPENDS ${SRC}
                COMMENT "Assembling ${SRC} -> ${OBJ}"
        )
    else()
        add_custom_command(
                OUTPUT ${OBJ}
                COMMAND ${CMAKE_C_COMPILER} -m32 -ffreestanding -fno-builtin -fno-stack-protector -nostdlib -c ${SRC} -o ${OBJ}
                DEPENDS ${SRC}
                COMMENT "Compiling ${SRC} -> ${OBJ}"
        )
    endif()

    list(APPEND OBJ_FILES ${OBJ})
endforeach()

# Target to build all object files
add_custom_target(objs ALL DEPENDS ${OBJ_FILES})
