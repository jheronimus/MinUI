// Generic Wi-Fi backend for Minime (iwd/iwctl).
// Tool dependencies: iwctl (from iwd), iproute2.
// Config: /mnt/sdcard/.minime/config/wifi.cfg (SSID=/Passphrase=), seeded
// into iwd profiles by the firmware `wifi` service (see boards/common).
// Enabled gate: /mnt/sdcard/.minime/config/wifi/enabled.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "traits.h"
#include "wireless.h"

#define WIFI_CONFIG_PATH "/mnt/sdcard/.minime/config/wifi.cfg"
#define WIFI_ENABLE_FILE "/mnt/sdcard/.minime/config/wifi/enabled"
#define WIFI_SERVICE "/etc/init.d/wifi"

#define WIFI_SCAN_CMD "iwctl station %s scan >/dev/null 2>&1"
#define WIFI_GET_NETWORKS_CMD "iwctl station %s get-networks 2>/dev/null"
#define WIFI_STATION_SHOW_CMD "iwctl station %s show 2>/dev/null"
#define WIFI_CONNECT_CMD "iwctl station %s connect \"%s\" >/dev/null 2>&1"
#define WIFI_DISCONNECT_CMD "iwctl station %s disconnect >/dev/null 2>&1"

static int wifi_enabled = 0;
static WifiNetwork networks[WIFI_MAX_NETWORKS];
static int network_count = 0;
static int scanning = 0;

static const char *wifi_interface(void) {
	const MinimeTraits *traits = MINIME_traits();

	return (traits && traits->wifi_interface[0] && strcmp(traits->wifi_interface, "na"))
	           ? traits->wifi_interface
	           : "wlan0";
}

int WIFI_hasWifi(void) {
	const MinimeTraits *traits = MINIME_traits();

	return traits && traits->wifi_interface[0] && strcmp(traits->wifi_interface, "na");
}

static int is_wifi_interface_present(void) {
	char path[256];

	if (!WIFI_hasWifi())
		return 0;
	snprintf(path, sizeof(path), "/sys/class/net/%s", wifi_interface());
	return access(path, F_OK) == 0;
}

static int is_wifi_admin_up(void) {
	char path[256];
	char flags[32] = {0};
	FILE *f;
	unsigned long value;

	snprintf(path, sizeof(path), "/sys/class/net/%s/flags", wifi_interface());
	f = fopen(path, "r");
	if (!f)
		return 0;
	if (!fgets(flags, sizeof(flags), f)) {
		fclose(f);
		return 0;
	}
	fclose(f);
	value = strtoul(flags, NULL, 0);
	return (value & 1) != 0;
}

static int is_wifi_connected(void) {
	char path[256];
	char carrier[8] = {0};
	FILE *f;

	snprintf(path, sizeof(path), "/sys/class/net/%s/carrier", wifi_interface());
	f = fopen(path, "r");
	if (!f)
		return 0;
	if (!fgets(carrier, sizeof(carrier), f)) {
		fclose(f);
		return 0;
	}
	fclose(f);
	return strncmp(carrier, "1", 1) == 0;
}

static FILE *wifi_popen(const char *cmd) {
	char shell[256];

	snprintf(shell, sizeof(shell), "%s", cmd);
	return popen(shell, "r");
}

static void strip_ansi(char *s);
static void trim(char *s);

static void get_connected_ssid(char *ssid_out, size_t max_len) {
	char cmd[256];
	char line[256];
	FILE *f;

	ssid_out[0] = '\0';
	snprintf(cmd, sizeof(cmd), WIFI_STATION_SHOW_CMD, wifi_interface());
	f = wifi_popen(cmd);
	if (!f)
		return;
	while (fgets(line, sizeof(line), f)) {
		strip_ansi(line);
		if (strstr(line, "Connected network")) {
			char *value = strstr(line, "Connected network") + strlen("Connected network");
			size_t len;

			while (*value == ' ' || *value == '\t')
				value++;
			len = strlen(value);
			while (len > 0 && (value[len - 1] == '\n' || value[len - 1] == '\r' || value[len - 1] == ' ')) {
				value[len - 1] = '\0';
				len--;
			}
			if (strcmp(value, "--") != 0)
				strncpy(ssid_out, value, max_len);
			break;
		}
	}
	pclose(f);
}

static int is_ssid_known(const char *ssid) {
	FILE *f = fopen(WIFI_CONFIG_PATH, "r");
	char line[128];
	int known = 0;

	if (!f || !ssid)
		return 0;
	while (fgets(line, sizeof(line), f)) {
		char *val;
		char *start = line;

		while (*start == ' ' || *start == '\t')
			start++;
		if (strncmp(start, "SSID=", 5) != 0)
			continue;
		val = start + 5;
		while (*val == ' ' || *val == '\t')
			val++;
		{
			size_t len = strlen(val);
			while (len > 0 && (val[len - 1] == '\n' || val[len - 1] == '\r' || val[len - 1] == ' ' || val[len - 1] == '\t')) {
				val[len - 1] = '\0';
				len--;
			}
			if (len >= 2 && val[0] == '"' && val[len - 1] == '"') {
				val[len - 1] = '\0';
				val++;
			}
		}
		if (strcmp(val, ssid) == 0) {
			known = 1;
			break;
		}
	}
	fclose(f);
	return known;
}

static void trim(char *s) {
	size_t len = strlen(s);

	while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r' || s[len - 1] == ' ')) {
		s[len - 1] = '\0';
		len--;
	}
	while (*s == ' ')
		s++;
}

// Strip ANSI escape sequences (iwctl colors the selected row and signal bars).
static void strip_ansi(char *s) {
	char *src = s;
	char *dst = s;

	while (*src) {
		if (*src == 0x1b) {
			// skip ESC [ ... letter
			src++;
			if (*src == '[') {
				src++;
				while (*src && !((*src >= 'A' && *src <= 'Z') || (*src >= 'a' && *src <= 'z')))
					src++;
				if (*src)
					src++;
				continue;
			}
		}
		*dst++ = *src++;
	}
	*dst = '\0';
}

// Parse `iwctl station <if> get-networks` output. Rows are:
//   [>] <name...> <security> <signal>
// The name may contain spaces, so parse from the right: the last token is the
// (unused) signal bars column, security is the second-to-last, everything
// before security is the SSID. The selected row may have a leading ">" and
// ANSI colors. Works on the raw line without destructive in-place edits.
static void parse_scan_results(void) {
	char cmd[256];
	char line[256];
	FILE *f;

	network_count = 0;
	snprintf(cmd, sizeof(cmd), WIFI_GET_NETWORKS_CMD, wifi_interface());
	f = wifi_popen(cmd);
	if (!f)
		return;

	while (fgets(line, sizeof(line), f) && network_count < WIFI_MAX_NETWORKS) {
		char *sec;
		char *sig;
		char *name_end;
		char *name_start;
		char *end;
		char *p;
		size_t len;
		WifiNetwork *net;

		strip_ansi(line);
		// skip header/separator/title rows
		if (strstr(line, "Network name") || strstr(line, "---") || strstr(line, "Available"))
			continue;

		len = strlen(line);
		while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r' || line[len - 1] == ' '))
			line[--len] = '\0';
		if (!len)
			continue;

		// skip the trailing signal column (bars, not meaningful in this iwd)
		end = line + len;
		sig = end;
		while (sig > line && sig[-1] != ' ' && sig[-1] != '\t')
			sig--;
		if (sig == end)
			continue;

		// security = second-to-last token
		sec = sig - 1;
		while (sec >= line && (*sec == ' ' || *sec == '\t'))
			sec--;
		sec++;
		p = sec;
		while (p > line && p[-1] != ' ' && p[-1] != '\t')
			p--;
		if (p == sec)
			continue;
		sec = p;
		// name = everything before security, trimmed and without ">"
		name_end = sec;
		while (name_end > line && (name_end[-1] == ' ' || name_end[-1] == '\t'))
			name_end--;
		name_start = line;
		while (*name_start == ' ' || *name_start == '\t' || *name_start == '>')
			name_start++;

		if (name_end <= name_start)
			continue;

		net = &networks[network_count];
		memset(net, 0, sizeof(*net));
		len = (size_t)(name_end - name_start);
		if (len >= sizeof(net->ssid))
			len = sizeof(net->ssid) - 1;
		memcpy(net->ssid, name_start, len);
		net->ssid[len] = '\0';

		net->security = (strstr(sec, "open") || !strcasecmp(sec, "open")) ? WIFI_SECURITY_OPEN
		                                                                  : WIFI_SECURITY_WPA;
		net->known = is_ssid_known(net->ssid);
		net->connected = 0;
		network_count++;
	}
	pclose(f);

	// mark the connected network
	{
		char connected[WIFI_MAX_SSID];
		int i;

		get_connected_ssid(connected, sizeof(connected));
		if (connected[0]) {
			for (i = 0; i < network_count; i++) {
				if (strcmp(networks[i].ssid, connected) == 0) {
					networks[i].connected = 1;
					break;
				}
			}
		}
	}
}

///////////////////////////////////////

int WIFI_init(void) {
	wifi_enabled = is_wifi_admin_up() || is_wifi_connected();
	network_count = 0;
	scanning = 0;
	return 0;
}

int WIFI_enabled(void) {
	// always re-check live state: iwd may bring the interface up/down
	// independently of this process (eg. auto-connect at boot, another UI)
	return is_wifi_admin_up() || is_wifi_connected();
}

int WIFI_setEnabled(int enabled) {
	FILE *f;
	char cmd[256];

	if (enabled) {
		// touch the gate so wifi persists across reboots
		snprintf(cmd, sizeof(cmd), "mkdir -p /mnt/sdcard/.minime/config/wifi");
		(void)system(cmd);
		f = fopen(WIFI_ENABLE_FILE, "w");
		if (f) {
			fputs("1\n", f);
			fclose(f);
		}
		// start the service detached so the menu never blocks on the (up to
		// 40s) connection wait; success is a running iwd, not instant connect
		snprintf(cmd, sizeof(cmd), WIFI_SERVICE " start >/dev/null 2>&1 &");
		(void)system(cmd);
		wifi_enabled = 1;
	} else {
		unlink(WIFI_ENABLE_FILE);
		snprintf(cmd, sizeof(cmd), WIFI_SERVICE " stop >/dev/null 2>&1 &");
		(void)system(cmd);
		wifi_enabled = 0;
	}
	return 0;
}

// iwd scans are asynchronous: `iwctl station scan` returns immediately and
// results populate over the next second or two. Fire the scan and return;
// WIFI_getNetworks() reads results on a later poll. Never block here — the
// caller runs inside the menu loop.
int WIFI_scan(void) {
	char cmd[256];

	if (!WIFI_enabled())
		return -1;
	scanning = 1;
	snprintf(cmd, sizeof(cmd), WIFI_SCAN_CMD, wifi_interface());
	(void)system(cmd);
	scanning = 0;
	return 0;
}

int WIFI_getNetworks(WifiNetwork *out, int max) {
	int i;

	if (!out)
		return 0;
	parse_scan_results();
	for (i = 0; i < network_count && i < max; i++)
		out[i] = networks[i];
	return network_count < max ? network_count : max;
}

int WIFI_connected(void) {
	return is_wifi_connected();
}

int WIFI_connect(const char *ssid, const char *passphrase) {
	FILE *f;
	char cmd[256];

	if (!ssid)
		return -1;

	// persist to wifi.cfg so the network is known across reboots (the wifi
	// service seeds iwd profiles from this file)
	f = fopen(WIFI_CONFIG_PATH, "a");
	if (f) {
		// avoid duplicates
		if (!is_ssid_known(ssid)) {
			fprintf(f, "\nSSID=%s\nPassphrase=%s\n", ssid, passphrase ? passphrase : "");
		}
		fclose(f);
	}
	snprintf(cmd, sizeof(cmd), "rc-service wifi reload >/dev/null 2>&1");
	(void)system(cmd);

	snprintf(cmd, sizeof(cmd), WIFI_CONNECT_CMD, wifi_interface(), ssid);
	return system(cmd) == 0 ? 0 : -1;
}

int WIFI_disconnect(void) {
	char cmd[256];

	snprintf(cmd, sizeof(cmd), WIFI_DISCONNECT_CMD, wifi_interface());
	(void)system(cmd);
	return 0;
}

int WIFI_forget(const char *ssid) {
	char cmd[256];
	FILE *f;
	FILE *out;
	char line[256];
	char current_ssid[128] = {0};
	char current_pass[128] = {0};

	if (!ssid)
		return -1;

	// remove from wifi.cfg
	f = fopen(WIFI_CONFIG_PATH, "r");
	if (f) {
		out = fopen(WIFI_CONFIG_PATH ".tmp", "w");
		if (!out) {
			fclose(f);
		} else {
			while (fgets(line, sizeof(line), f)) {
				char *key = line;
				char *val = strchr(line, '=');

				if (!val) {
					fprintf(out, "%s", line);
					continue;
				}
				*val = '\0';
				val++;
				{
					size_t len = strlen(val);
					while (len > 0 && (val[len - 1] == '\n' || val[len - 1] == '\r')) {
						val[len - 1] = '\0';
						len--;
					}
				}
				if (strcmp(key, "SSID") == 0) {
					if (current_ssid[0] != '\0' && strcmp(current_ssid, ssid) != 0) {
						fprintf(out, "SSID=%s\n", current_ssid);
						if (current_pass[0] != '\0')
							fprintf(out, "Passphrase=%s\n", current_pass);
						fprintf(out, "\n");
					}
					current_pass[0] = '\0';
					strncpy(current_ssid, val, sizeof(current_ssid));
				} else if (strcmp(key, "Passphrase") == 0) {
					strncpy(current_pass, val, sizeof(current_pass));
				} else {
					*(val - 1) = '=';
					fprintf(out, "%s=%s\n", key, val);
				}
			}
			if (current_ssid[0] != '\0' && strcmp(current_ssid, ssid) != 0) {
				fprintf(out, "SSID=%s\n", current_ssid);
				if (current_pass[0] != '\0')
					fprintf(out, "Passphrase=%s\n", current_pass);
			}
			fclose(out);
		}
		fclose(f);
		rename(WIFI_CONFIG_PATH ".tmp", WIFI_CONFIG_PATH);
	}

	snprintf(cmd, sizeof(cmd), "iwctl known-networks forget \"%s\" >/dev/null 2>&1", ssid);
	(void)system(cmd);
	snprintf(cmd, sizeof(cmd), "rc-service wifi reload >/dev/null 2>&1");
	(void)system(cmd);
	return 0;
}

int WIFI_isKnown(const char *ssid) {
	return is_ssid_known(ssid);
}

int WIFI_isBusy(void) {
	return scanning;
}
