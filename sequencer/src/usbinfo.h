#ifndef SEQ4H_USBINFO_H
#define SEQ4H_USBINFO_H

#include <stddef.h>

/* What the handheld sees on its USB port, read from /sys/bus/usb/devices
 * (only the plain attributes: idVendor, product, bInterfaceClass...). Used by
 * M > MIDI and the log so a cable that does not work can be diagnosed on the
 * device: is it seen at all, is it a USB MIDI device, did a driver take it. */
typedef struct {
    char path[16];      /* sysfs name, e.g. "1-1" or "1-1.2" (behind a hub) */
    char id[10];        /* "1a86:752d" */
    char name[48];      /* manufacturer and product, as the device reports them */
    int hub;            /* a USB hub, not a device */
    int midi;           /* has a MIDI streaming interface (class 01, subclass 03) */
    int audio;          /* has any audio-class interface */
    char driver[24];    /* driver bound to the MIDI (or first audio) interface, "" = none */
} UsbDev;

/* Fills out with up to max devices (root hubs left out) and returns how many.
 * root = NULL uses $SEQ4H_USB_DIR if set (for tests), else /sys/bus/usb/devices. */
int usb_scan(const char *root, UsbDev *out, int max);

/* One line for the log: "1-1 1a86:752d USB2.0-MIDI: MIDI, driver snd-usb-audio". */
void usb_describe(const UsbDev *d, char *buf, size_t n);

#endif
