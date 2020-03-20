################################################################################
# Check packages
################################################################################

include(FindPkgConfig)

pkg_check_modules (V4L2 REQUIRED libv4l2)

pkg_check_modules (MEDIACTL REQUIRED libmediactl)

pkg_check_modules(CONFIG REQUIRED libconfig++)

################################################################################
# Includes
################################################################################

include_directories(
	.
	include_directories(${V4L2_INCLUDE_DIR})
)

################################################################################
# Sources
################################################################################

set(SOURCES
	main.cpp
	Backend.cpp
	Camera.cpp
	CameraHandler.cpp
	CameraManager.cpp
	CommandHandler.cpp
	FrontendBuffer.cpp
	V4L2ToXen.cpp
	MediaController.cpp
	Config.cpp
)

################################################################################
# Targets
################################################################################

add_executable(${PROJECT_NAME} ${SOURCES})

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)

################################################################################
# Libraries
################################################################################

set(XENBE_LIB xenbe)

target_link_libraries(${PROJECT_NAME}
	${XENBE_LIB}
	${V4L2_LIBRARY}
	${MEDIACTL_LIBRARIES}
	v4l2subdev
	${CONFIG_LIBRARIES}
	pthread
)
