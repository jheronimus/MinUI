#ifndef WIRELESS_H
#define WIRELESS_H

///////////////////////////////

#include <stddef.h>

///////////////////////////////

#define WIFI_MAX_SSID 64
#define WIFI_MAX_NETWORKS 16
#define BT_MAX_ADDR 18
#define BT_MAX_NAME 64
#define BT_MAX_DEVICES 16

typedef enum {
	WIFI_SECURITY_OPEN = 0,
	WIFI_SECURITY_WPA,
} WifiSecurity;

typedef struct {
	char ssid[WIFI_MAX_SSID];
	WifiSecurity security;
	int connected;
	int known;
} WifiNetwork;

typedef enum {
	BT_DEVICE_UNKNOWN = 0,
	BT_DEVICE_AUDIO,
	BT_DEVICE_GAMEPAD,
} BtDeviceKind;

typedef struct {
	char addr[BT_MAX_ADDR];
	char name[BT_MAX_NAME];
	BtDeviceKind kind;
	int paired;
	int connected;
} BtDevice;

///////////////////////////////

// Wi-Fi (iwd backend)
int WIFI_init(void);
int WIFI_enabled(void);
int WIFI_setEnabled(int enabled);
int WIFI_scan(void); // triggers a scan
int WIFI_getNetworks(WifiNetwork* networks, int max); // last scan results
int WIFI_connect(const char* ssid, const char* passphrase);
int WIFI_disconnect(void);
int WIFI_forget(const char* ssid);

// Bluetooth (bluetoothctl backend)
int BT_init(void);
int BT_quit(void);
int BT_enabled(void);
int BT_setEnabled(int enabled);
int BT_scan(void); // triggers discovery
int BT_getDevices(BtDevice* devices, int max); // discovered + paired devices
int BT_toggleDevice(const char* addr);
int BT_forgetDevice(const char* addr);
int BT_isBusy(void);

///////////////////////////////

#endif
