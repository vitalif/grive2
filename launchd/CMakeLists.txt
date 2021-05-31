SET(GRIVE_SYNC_SH_BINARY "${CMAKE_INSTALL_PREFIX}/lib/grive/grive-sync.sh")

CONFIGURE_FILE(
	com.github.vitalif.grive2.grive-changes.plist.in
	com.github.vitalif.grive2.grive-changes.plist
	@ONLY
)
CONFIGURE_FILE(
	com.github.vitalif.grive2.grive-timer.plist.in
	com.github.vitalif.grive2.grive-timer.plist
	@ONLY
)

install(
	FILES
		${CMAKE_BINARY_DIR}/launchd/com.github.vitalif.grive2.grive-changes.plist
		${CMAKE_BINARY_DIR}/launchd/com.github.vitalif.grive2.grive-timer.plist
	DESTINATION
		lib/launchd
)

install(
	PROGRAMS
		../systemd/grive-sync.sh
	DESTINATION
		lib/grive
)
