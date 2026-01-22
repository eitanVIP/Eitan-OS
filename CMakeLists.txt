cmake_minimum_required(VERSION 3.16)
project(eitanos C ASM_NASM)

enable_language(ASM_NASM)

file(GLOB SRC_FILES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/*.c" "${CMAKE_SOURCE_DIR}/src/*.S")

set(OBJ_FILES "")

foreach(SRC ${SRC_FILES})
    get_filename_component(NAME ${SRC} NAME_WE)
    set(OBJ "${CMAKE_BINARY_DIR}/${NAME}.o")

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

add_custom_target(objs ALL DEPENDS ${OBJ_FILES})

# 👇 This is the magic line: add a "fake" target for CLion's code model
add_executable(fake_target ${SRC_FILES}
        src/filesystem.c
        src/filesystem.h
        src/stdint.h
        src/program_loader.c
        src/program_loader.h)

# Pass the same compiler flags so CLion knows how to parse code
target_compile_options(fake_target PRIVATE -m32 -ffreestanding -fno-builtin -fno-stack-protector -nostdlib)
set_target_properties(fake_target PROPERTIES EXCLUDE_FROM_ALL TRUE) # don't build it by default
