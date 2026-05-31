set(incs include)
set(srcs "")

if (CONFIG_BEKEN_KWS)
	list(APPEND srcs
		src/bk_kws.cc
	)
endif()


armino_component_register(
	SRCS "${srcs}"
	INCLUDE_DIRS "${incs}"
)

if (CONFIG_BEKEN_KWS)
	get_filename_component(lib_name ${CMAKE_CURRENT_SOURCE_DIR}/bk7258/libkws_model.a NAME)
	add_prebuilt_library(${lib_name} "${CMAKE_CURRENT_SOURCE_DIR}/bk7258/libkws_model.a")
	target_link_libraries(${COMPONENT_LIB} INTERFACE ${lib_name})

	get_filename_component(lib_name ${CMAKE_CURRENT_SOURCE_DIR}/bk7258/libtensorflow-microlite.a NAME)
	add_prebuilt_library(${lib_name} "${CMAKE_CURRENT_SOURCE_DIR}/bk7258/libtensorflow-microlite.a")
	target_link_libraries(${COMPONENT_LIB} INTERFACE ${lib_name})
endif()
