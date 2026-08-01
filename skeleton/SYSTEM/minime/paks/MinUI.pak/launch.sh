#!/bin/sh

export PLATFORM="minime"
export SDCARD_PATH="/mnt/sdcard"
export BIOS_PATH="$SDCARD_PATH/Bios"
export SAVES_PATH="$SDCARD_PATH/Saves"
export SYSTEM_PATH="$SDCARD_PATH/.system/$PLATFORM"
export CORES_PATH="$SYSTEM_PATH/cores"
export USERDATA_PATH="$SDCARD_PATH/.userdata/$PLATFORM"
export SHARED_USERDATA_PATH="$SDCARD_PATH/.userdata/shared"
export LOGS_PATH="$USERDATA_PATH/logs"
export DATETIME_PATH="$SHARED_USERDATA_PATH/datetime.txt"
#######################################

export PATH=$SYSTEM_PATH/bin:$PATH
export LD_LIBRARY_PATH=$SYSTEM_PATH/lib:$LD_LIBRARY_PATH

#######################################

#######################################

keymon & # > $LOGS_PATH/keymon.txt 2>&1 &

#######################################

mkdir -p "$LOGS_PATH"
mkdir -p "$SHARED_USERDATA_PATH/.minui"
AUTO_PATH="$USERDATA_PATH/auto.sh"
if [ -f "$AUTO_PATH" ]; then
	"$AUTO_PATH" # > $LOGS_PATH/auto.txt 2>&1
fi

cd $(dirname "$0")

#######################################

EXEC_PATH="/tmp/minui_exec"
NEXT_PATH="/tmp/next"
touch "$EXEC_PATH" && sync
while [ -f "$EXEC_PATH" ]; do
	minui > $LOGS_PATH/minui.txt 2>&1
	echo `date +'%F %T'` > "$DATETIME_PATH"
	sync
	
	if [ -f $NEXT_PATH ]; then
		. $NEXT_PATH
		rm -f $NEXT_PATH
		echo `date +'%F %T'` > "$DATETIME_PATH"
		sync
	fi
done

poweroff
