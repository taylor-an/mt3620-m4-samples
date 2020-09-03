#  Copyright (c) Codethink Ltd. All rights reserved.
#  Licensed under the MIT License.

cmake_minimum_required(VERSION 3.11)
project(SPI_SDCard_RTApp_MT3620_BareMetal C)

add_link_options(-specs=nano.specs -specs=nosys.specs)

# Create executable
add_executable(${PROJECT_NAME}
	main.c
	SD.c
	fatfs.c
	general_sdcard.c
	user_diskio.c

	lib/VectorTable.c
	lib/GPT.c
	lib/GPIO.c
	lib/UART.c
	lib/Print.c
	lib/SPIMaster.c

	Middlewares/Third_Party/FatFs/src/diskio.c
	Middlewares/Third_Party/FatFs/src/ff.c
	Middlewares/Third_Party/FatFs/src/ff_gen_drv.c
	Middlewares/Third_Party/FatFs/src/option/ccsbcs.c
	)

# Include Folders
target_include_directories(${PROJECT_NAME} PUBLIC
	./
	./Middlewares/Third_Party/FatFs/src
	)

target_link_libraries(${PROJECT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES LINK_DEPENDS ${CMAKE_SOURCE_DIR}/linker.ld)

azsphere_configure_tools(TOOLS_REVISION "20.07")

# Add MakeImage post-build command
azsphere_target_add_image_package(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME} PUBLIC -L"${CMAKE_SOURCE_DIR}")
