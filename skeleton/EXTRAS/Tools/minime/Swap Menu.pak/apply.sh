#!/bin/bash

# Swap the logical MENU and SELECT button codes in the Minime traits file.
# Reversible: run again to swap back. Takes effect after reboot.

killshow()
{
	SHOW_ID=`pidof show.elf`
	kill $SHOW_ID
	wait $SHOW_ID 2>/dev/null
}

DIR="$(dirname "$0")"
cd "$DIR"

show.elf "$DIR/res/preparing.png" 60 &

TRAITS_FILE=/mnt/sdcard/.minime/traits

if [ ! -f "$TRAITS_FILE" ]; then
	killshow
	show.elf "$DIR/res/fail.png" 2
	echo "missing traits file, aborting"
	exit 1
fi

KEY_MENU=$(sed -n 's/^key_menu=//p' "$TRAITS_FILE" | head -n 1)
KEY_SELECT=$(sed -n 's/^key_select=//p' "$TRAITS_FILE" | head -n 1)

if ! echo "$KEY_MENU" | grep -Eq '^[0-9]+$' || ! echo "$KEY_SELECT" | grep -Eq '^[0-9]+$'; then
	killshow
	show.elf "$DIR/res/fail.png" 2
	echo "bad key codes ($KEY_MENU/$KEY_SELECT), aborting"
	exit 1
fi

if [ "$KEY_MENU" = "$KEY_SELECT" ]; then
	killshow
	show.elf "$DIR/res/fail.png" 2
	echo "menu and select are already identical, nothing to swap"
	exit 1
fi

# swap the codes
sed -i "s/^key_menu=.*/key_menu=$KEY_SELECT/" "$TRAITS_FILE"
sed -i "s/^key_select=.*/key_select=$KEY_MENU/" "$TRAITS_FILE"

# verify the swap took effect
NEW_MENU=$(sed -n 's/^key_menu=//p' "$TRAITS_FILE" | head -n 1)
NEW_SELECT=$(sed -n 's/^key_select=//p' "$TRAITS_FILE" | head -n 1)
if [ "$NEW_MENU" != "$KEY_SELECT" ] || [ "$NEW_SELECT" != "$KEY_MENU" ]; then
	killshow
	show.elf "$DIR/res/fail.png" 2
	echo "swap failed, aborting"
	exit 1
fi

sync

killshow
show.elf "$DIR/res/applying.png" 60 &

echo "swapped menu ($KEY_MENU->$NEW_MENU) and select ($KEY_SELECT->$NEW_SELECT)"
echo "rebooting to apply"
sleep 3
reboot
