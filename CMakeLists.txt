cmake_minimum_required(VERSION 3.16)
project(TFhelloworld)

set(CMAKE_CXX_STANDARD 14)

set(THE_FORGE_PATH "E:/theforgeworkspace/The-Forge" CACHE PATH "The-Forge path")
set(THE_FORGE_USE_DX12 ON  CACHE BOOL "Link DX12")
set(THE_FORGE_USE_VK   OFF CACHE BOOL "Link Vulkan")

message(${THE_FORGE_PATH})
message(${THE_FORGE_USE_DX12})
message(${THE_FORGE_USE_VK})

include_directories(${THE_FORGE_PATH})

list(APPEND THE_FORGE_LIBS
        Xinput9_1_0
        ws2_32)

if (${THE_FORGE_USE_DX12})
    message("THE_FORGE_USE_DX12")
    link_directories(${THE_FORGE_PATH}/Examples_3/Unit_Tests/PC\ Visual\ Studio\ 2017/x64/DebugDx)
    list(APPEND THE_FORGE_LIBS
#            amd_ags_x64
#            WinPixEventRunTime
#            dxcompiler
#            dxil
            gainputstatic
            OS
            RendererDX12
            )

    add_definitions(
            -DUSE_MEMORY_TRACKING
            -D_DEBUG
            -D_WINDOWS
            -DDIRECT3D12)

    add_custom_target(DX12copyDLL
            COMMAND ${CMAKE_COMMAND} -E copy ${THE_FORGE_PATH}/Examples_3/Unit_Tests/PC\ Visual\ Studio\ 2017/x64/DebugDx/amd_ags_x64.dll          ${CMAKE_BINARY_DIR}
            COMMAND ${CMAKE_COMMAND} -E copy ${THE_FORGE_PATH}/Examples_3/Unit_Tests/PC\ Visual\ Studio\ 2017/x64/DebugDx/WinPixEventRuntime.dll   ${CMAKE_BINARY_DIR}
            COMMAND ${CMAKE_COMMAND} -E copy ${THE_FORGE_PATH}/Examples_3/Unit_Tests/PC\ Visual\ Studio\ 2017/x64/DebugDx/dxcompiler.dll           ${CMAKE_BINARY_DIR}
            COMMAND ${CMAKE_COMMAND} -E copy ${THE_FORGE_PATH}/Examples_3/Unit_Tests/PC\ Visual\ Studio\ 2017/x64/DebugDx/dxil.dll                 ${CMAKE_BINARY_DIR})

endif ()

if (${THE_FORGE_USE_VK})
    message("THE_FORGE_USE_VK")
    find_package(Vulkan REQUIRED)

    IF (NOT Vulkan_FOUND)
        message(FATAL_ERROR "Could not find Vulkan library!")
    ELSE()
        message(STATUS ${Vulkan_LIBRARY})
    ENDIF()

    include_directories(${Vulkan_INCLUDE_DIRS})
    link_directories(${Vulkan_LIBRARY})
    link_directories(${THE_FORGE_PATH}/Examples_3/Unit_Tests/PC\ Visual\ Studio\ 2017/x64/DebugVk)
    list(APPEND THE_FORGE_LIBS
            gainputstatic
            OS
            RendererVulkan
            SpirvTools
            ${Vulkan_LIBRARIES})
    add_definitions(
            -DUSE_MEMORY_TRACKING
            -D_DEBUG
            -D_WINDOWS
            -DVULKAN
    )
endif ()

#message(${THE_FORGE_LIBS})

function(TFexe exename srcfiles)
    add_executable(${exename} ${srcfiles})
    target_link_libraries(${exename} ${THE_FORGE_LIBS})
    set_target_properties(${exename} PROPERTIES LINK_FLAGS "/ignore:4099")
    if (${THE_FORGE_USE_DX12})
        add_dependencies(${exename} DX12copyDLL)
    endif ()
endfunction()

# Exmaples
TFexe(00-Window Examples/01-Triangle/00-window.cpp)
TFexe(01-Triangle Examples/01-Triangle/01-triangle.cpp)
TFexe(01.1-Triangle-Append Examples/01-Triangle/01.1-triangle-append.cpp)
TFexe(02-Texture Examples/02-Texture/02-texture.cpp)
TFexe(03-Compute Examples/03-Compute/03-compute.cpp)
TFexe(04-RaytracingTriangle Examples/04-RaytracingTriangle/04-raytracing-triangle.cpp)
