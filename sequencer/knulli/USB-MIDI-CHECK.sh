#!/bin/sh
# Ports-menu diagnostic for USB MIDI on the RG40XX-V under Knulli.
# Copy to /userdata/roms/ports/ next to GENVOLVE.sh and run it from the Ports menu
# (or over SSH). It changes nothing except loading sound modules that are
# already on the device, and writes /userdata/roms/ports/genvolve/usb-midi-check.log.
# Run it once with the FM1 unplugged and once plugged in, then compare.
#
# Robustness rules (the first version stopped after the USB role section):
# - no `set -e`; every section runs even if an earlier one fails;
# - every external command runs under a time limit, so one blocking read of a
#   sysfs attribute (sunxi's usbc0/usb_host did this) or a hung tool can't
#   stall the rest;
# - only known-safe sysfs attributes are read; others are listed by name;
# - every missing tool (lsusb, zcat, aconnect, amidi, modprobe...) has a
#   fallback or a plain "not available" line;
# - the log is synced to the SD card after every section, so a power-off or
#   a killed launcher still leaves everything written so far.

DIR="$(dirname "$0")/genvolve"
mkdir -p "$DIR" 2>/dev/null
LOG="$DIR/usb-midi-check.log"

have() { command -v "$1" >/dev/null 2>&1; }

# t SECONDS CMD...: run CMD with a time limit when `timeout` exists.
if have timeout; then
    t() { s=$1; shift; timeout "$s" "$@"; }
elif have busybox && busybox timeout -t 1 true >/dev/null 2>&1; then
    t() { s=$1; shift; busybox timeout -t "$s" "$@"; }
else
    t() { shift; "$@"; }
fi

# rd FILE: print a sysfs/proc file, never blocking for more than 2 s.
rd() { [ -r "$1" ] || return 1; t 2 cat "$1" 2>/dev/null | tr -d '\0'; }

# gunzip-to-stdout with whatever exists.
gzcat() {
    if have zcat; then zcat "$1"
    elif have gzip; then gzip -dc "$1"
    elif have busybox; then busybox zcat "$1" || busybox gzip -dc "$1"
    else echo "(no zcat, gzip or busybox to read $1)"; return 1
    fi
}

section() {
    sync 2>/dev/null
    echo "--- $*"
}

{
echo "=== $(date 2>/dev/null)"
echo "script: USB-MIDI-CHECK v3"

section "system"
uname -a 2>/dev/null
rd /etc/os-release | grep -E '^(NAME|VERSION)='
rd /usr/share/batocera/batocera.version

section "USB controllers and role (otg/host/peripheral)"
for f in /sys/class/usb_role/*/role /sys/devices/platform/soc/*/musb-hdrc*/mode \
         /sys/devices/platform/soc*/usbc0/otg_role; do
    [ -e "$f" ] && echo "$f: $(rd "$f")"
done
for c in /sys/devices/platform/soc*/usbc0; do
    [ -d "$c" ] && echo "$c attributes (not read): $(ls "$c" 2>/dev/null | tr '\n' ' ')"
done
for u in /sys/class/udc/*; do
    [ -e "$u" ] && echo "udc (device-mode controller): $(basename "$u") state=$(rd "$u/state")"
done
echo "host controllers:"
for h in /sys/bus/usb/devices/usb*; do
    [ -e "$h" ] && echo "  $(basename "$h") $(rd "$h/product") speed=$(rd "$h/speed")"
done

section "power (read only: battery, USB input, VBUS regulators, what devices ask for)"
for ps in /sys/class/power_supply/*; do
    [ -d "$ps" ] || continue
    echo "$(basename "$ps"):"
    for a in type online present status capacity voltage_now current_now input_current_limit \
             constant_charge_current otg_id usb_type; do
        v=$(rd "$ps/$a") && echo "  $a=$v"
    done
done
for rg in /sys/class/regulator/*; do
    [ -d "$rg" ] || continue
    n=$(rd "$rg/name")
    case "$n" in *vbus*|*VBUS*|*drivebus*|*usb*|*USB*|*otg*) echo "regulator $(basename "$rg") $n state=$(rd "$rg/state") microvolts=$(rd "$rg/microvolts")" ;; esac
done
for d in /sys/bus/usb/devices/*; do
    [ -e "$d/bMaxPower" ] && echo "$(basename "$d") $(rd "$d/product") asks for bMaxPower=$(rd "$d/bMaxPower")"
done

section "USB devices"
n=0
for d in /sys/bus/usb/devices/*; do
    [ -f "$d/idVendor" ] || continue
    n=$((n + 1))
    echo "$(basename "$d")  $(rd "$d/idVendor"):$(rd "$d/idProduct")  $(rd "$d/manufacturer") $(rd "$d/product")  power=$(rd "$d/bMaxPower") speed=$(rd "$d/speed")"
    for i in "$d"/*:*; do
        [ -f "$i/bInterfaceClass" ] || continue
        drv=""
        [ -e "$i/driver" ] && drv=$(basename "$(readlink "$i/driver" 2>/dev/null)")
        echo "    $(basename "$i") class=$(rd "$i/bInterfaceClass") sub=$(rd "$i/bInterfaceSubClass") driver=${drv:-none}"
    done
done
echo "$n USB device(s) (class 01 sub 03 = MIDI streaming, 01 sub 01 = audio control)"
if have lsusb; then t 5 lsusb 2>&1; else echo "(lsusb not installed; list above is from /sys)"; fi

section "kernel sound config"
CFG_RE='CONFIG_SND_(USB_AUDIO|USBMIDI|RAWMIDI|SEQUENCER|SEQ_|HWDEP)|CONFIG_USB_(OTG|MUSB|EHCI|OHCI)|CONFIG_USB_ROLE'
if [ -r /proc/config.gz ]; then
    gzcat /proc/config.gz 2>&1 | grep -E "$CFG_RE" || echo "(no matching lines in /proc/config.gz)"
else
    found=""
    for c in "/boot/config-$(uname -r)" "/lib/modules/$(uname -r)/config"; do
        [ -r "$c" ] && { echo "from $c:"; grep -E "$CFG_RE" "$c"; found=1; }
    done
    [ -n "$found" ] || echo "/proc/config.gz not available (try: modprobe configs)"
fi
echo "built-in sound/USB drivers (modules.builtin):"
rd "/lib/modules/$(uname -r)/modules.builtin" | grep -E 'snd-usb|snd-seq|snd-rawmidi|snd-hwdep|usbmidi' \
    || echo "  (none listed or file missing)"

section "modules on disk"
if [ -d "/lib/modules/$(uname -r)" ]; then
    t 10 find "/lib/modules/$(uname -r)" \( -name 'snd-usb*' -o -name 'snd-seq*' \
        -o -name 'snd-rawmidi*' -o -name 'snd-hwdep*' \) 2>/dev/null
else
    echo "/lib/modules/$(uname -r) missing"
fi
for k in "$DIR"/*.ko "$DIR"/modules/*.ko; do [ -e "$k" ] && echo "bundled: $k"; done

section "loading modules (errors are fine if built in)"
if have modprobe; then
    for m in snd-usb-audio snd-seq snd-seq-midi; do
        t 10 modprobe "$m" 2>&1 | sed "s/^/$m: /"
    done
else
    echo "(modprobe not available)"
fi
if [ -r /proc/modules ]; then
    grep -E '^(snd|usb)' /proc/modules | cut -d' ' -f1-3 || echo "(no snd/usb modules loaded)"
fi

section "ALSA"
echo "cards:"
rd /proc/asound/cards || echo "(no /proc/asound/cards)"
echo "devices:"
rd /proc/asound/devices
ls -l /dev/snd 2>/dev/null || echo "(no /dev/snd)"
echo "rawmidi (USB MIDI ports show here even without the sequencer):"
for r in /proc/asound/card*/midi*; do [ -e "$r" ] && { echo "$r:"; rd "$r"; }; done
echo "sequencer clients:"
rd /proc/asound/seq/clients || echo "(none: no sequencer)"
if have aconnect; then t 5 aconnect -l 2>&1; else echo "(aconnect not installed)"; fi
if have amidi; then t 5 amidi -l 2>&1; else echo "(amidi not installed)"; fi
if [ -x "$DIR/genvolve" ]; then
    echo "genvolve --midi list:"
    t 10 "$DIR/genvolve" --midi list 2>&1
fi

section "kernel log (USB and sound)"
if have dmesg; then
    t 10 dmesg 2>/dev/null | grep -iE 'usb|midi|snd|otg|musb|vbus' | tail -40
else
    echo "(dmesg not available)"
fi

section "end"
echo
} >>"$LOG" 2>&1
sync 2>/dev/null
exit 0
