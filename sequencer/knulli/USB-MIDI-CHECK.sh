#!/bin/sh
# Ports-menu diagnostic for USB MIDI on the RG40XX-V under Knulli.
# Copy to /userdata/roms/ports/ next to SEQ4H.sh and run it from the Ports menu
# (or over SSH). It changes nothing except loading sound modules that are
# already on the device, and writes /userdata/roms/ports/seq4h/usb-midi-check.log.
# Run it once with the FM1 unplugged and once plugged in, then compare.
DIR="$(dirname "$0")/seq4h"
mkdir -p "$DIR"
LOG="$DIR/usb-midi-check.log"
{
echo "=== $(date)"
echo "--- system"
uname -a
cat /etc/os-release 2>/dev/null | grep -E '^(NAME|VERSION)='
cat /usr/share/batocera/batocera.version 2>/dev/null
echo "--- USB controllers and role (otg/host/peripheral)"
for f in /sys/class/usb_role/*/role /sys/devices/platform/soc/*/musb-hdrc*/mode \
         /sys/devices/platform/soc*/usbc0/otg_role /sys/devices/platform/soc*/usbc0/usb_host; do
    [ -e "$f" ] && echo "$f: $(cat "$f" 2>/dev/null)"
done
ls /sys/class/udc 2>/dev/null | sed 's/^/udc (device-mode controller): /'
echo "--- USB devices"
for d in /sys/bus/usb/devices/*; do
    [ -f "$d/idVendor" ] || continue
    echo "$(basename "$d")  $(cat "$d/idVendor"):$(cat "$d/idProduct")  $(cat "$d/manufacturer" 2>/dev/null) $(cat "$d/product" 2>/dev/null)  power=$(cat "$d/bMaxPower" 2>/dev/null)"
done
command -v lsusb >/dev/null && lsusb
echo "--- kernel sound config"
if [ -r /proc/config.gz ]; then
    zcat /proc/config.gz | grep -E 'CONFIG_SND_(USB_AUDIO|USBMIDI|RAWMIDI|SEQUENCER|SEQ_|HWDEP)|CONFIG_USB_(OTG|MUSB|EHCI|OHCI)|CONFIG_USB_ROLE'
else
    echo "/proc/config.gz not available"
fi
echo "--- modules on disk"
find /lib/modules/"$(uname -r)" -name 'snd-usb*' -o -name 'snd-seq*' -o -name 'snd-rawmidi*' 2>/dev/null
echo "--- loading modules (errors are fine if built in)"
for m in snd-usb-audio snd-seq snd-seq-midi; do modprobe "$m" 2>&1 | sed "s/^/$m: /"; done
lsmod 2>/dev/null | grep -E 'snd|usb'
echo "--- ALSA"
cat /proc/asound/cards 2>/dev/null
ls -l /dev/snd 2>/dev/null
echo "sequencer clients:"
cat /proc/asound/seq/clients 2>/dev/null || echo "(none: no sequencer)"
command -v aconnect >/dev/null && aconnect -l
command -v amidi >/dev/null && amidi -l
[ -x "$DIR/seq4h" ] && { echo "seq4h --midi list:"; "$DIR/seq4h" --midi list; }
echo "--- kernel log (USB and sound)"
dmesg 2>/dev/null | grep -iE 'usb|midi|snd|otg|musb|vbus' | tail -40
echo
} >>"$LOG" 2>&1
