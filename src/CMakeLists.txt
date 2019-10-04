AUX_SOURCE_DIRECTORY(. src_files)

ADD_EXECUTABLE(sigma-tcp ${src_files})

INSTALL(TARGETS sigma-tcp
	RUNTIME DESTINATION bin
)
