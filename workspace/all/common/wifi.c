// MinUI Wi-Fi backend for Minime (iwd).
// Tool dependencies: iwctl / iwd D-Bus.
// Known networks are persisted natively by iwd as .psk profiles in
// /mnt/sdcard/.minime/config/iwd.
// Enabled gate: /mnt/sdcard/.minime/config/wifi/enabled.

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "platform.h"
#include "wireless.h"
#include "utils.h"

#define IWD_STATE_DIR "/mnt/sdcard/.minime/config/iwd"
#define WIFI_ENABLE_FILE "/mnt/sdcard/.minime/config/wifi/enabled"
#define WIFI_SERVICE "/etc/init.d/wifi"

#define WIFI_SCAN_CMD "iwctl station %s scan >/dev/null 2>&1 &"
#define WIFI_GET_NETWORKS_CMD "iwctl station %s get-networks 2>/dev/null"
#define WIFI_DISCONNECT_CMD "iwctl station %s disconnect >/dev/null 2>&1 &"

static int wifi_enabled = 0;
static WifiNetwork networks[WIFI_MAX_NETWORKS];
static int network_count = 0;

static const char *wifi_interface(void) {
return PLAT_getWifiInterface();
}

static int WIFI_hasWifi(void) {
return PLAT_hasWifi();
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

static void strip_ansi(char *s) {
	char *src = s;
	char *dst = s;

	while (*src) {
		if (*src == 0x1b) {
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

// Load all known SSIDs directly from iwd profile state directory.
#define MAX_KNOWN_SSIDS 32
static char known_ssids[MAX_KNOWN_SSIDS][WIFI_MAX_SSID];
static int known_count = 0;

static void load_known_ssids(void) {
	DIR *d = opendir(IWD_STATE_DIR);
	struct dirent *ent;

	known_count = 0;
	if (!d)
		return;

	while ((ent = readdir(d)) != NULL && known_count < MAX_KNOWN_SSIDS) {
		char *dot = strstr(ent->d_name, ".psk");
		if (!dot || strcmp(dot, ".psk") != 0)
			continue;
		size_t len = (size_t)(dot - ent->d_name);
		if (ent->d_name[0] == '=') {
			// Hex-encoded SSID
			char *hex = ent->d_name + 1;
			size_t hlen = len - 1;
			char decoded[WIFI_MAX_SSID] = {0};
			size_t out_len = 0;
			for (size_t i = 0; i + 1 < hlen && out_len + 1 < sizeof(decoded); i += 2) {
				unsigned int byte_val = 0;
				if (sscanf(hex + i, "%02x", &byte_val) == 1)
					decoded[out_len++] = (char)byte_val;
			}
			decoded[out_len] = '\0';
			if (decoded[0]) {
				strncpy(known_ssids[known_count], decoded, sizeof(known_ssids[known_count]) - 1);
				known_ssids[known_count][sizeof(known_ssids[known_count]) - 1] = '\0';
				known_count++;
			}
		} else if (len > 0 && len < WIFI_MAX_SSID) {
			strncpy(known_ssids[known_count], ent->d_name, len);
			known_ssids[known_count][len] = '\0';
			known_count++;
		}
	}
	closedir(d);
}

static int is_ssid_known_cached(const char *ssid) {
	int i;
	if (!ssid || !ssid[0])
		return 0;
	for (i = 0; i < known_count; i++) {
		if (strcmp(known_ssids[i], ssid) == 0)
			return 1;
	}
	return 0;
}

// Parse `iwctl station <if> get-networks` output in a single pass.
static void parse_scan_results(void) {
	char cmd[256];
	char line[256];
	FILE *f;
	int connected_link = is_wifi_connected();

	network_count = 0;
	load_known_ssids();

	snprintf(cmd, sizeof(cmd), WIFI_GET_NETWORKS_CMD, wifi_interface());
	f = cmdOutput(cmd);
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
		int is_active = 0;
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

		// Check for active/connected marker (leading ">" or "*").
		// iwctl indents the marker with spaces before the ANSI escape
		// codes, so the marker is not at position 0 even after strip_ansi.
		p = line;
		while (*p == ' ' || *p == '\t')
			p++;
		if (*p == '>' || *p == '*')
			is_active = 1;

		// skip the trailing signal column
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
		while (*name_start == ' ' || *name_start == '\t' || *name_start == '>' || *name_start == '*')
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
		net->known = is_ssid_known_cached(net->ssid);
		net->connected = is_active && connected_link;
		network_count++;
	}
	pclose(f);
}

///////////////////////////////////////

int WIFI_init(void) {
	wifi_enabled = is_wifi_admin_up() || is_wifi_connected();
	network_count = 0;
	return 0;
}

int WIFI_enabled(void) {
	return is_wifi_admin_up() || is_wifi_connected();
}

int WIFI_setEnabled(int enabled) {
	FILE *f;
	char cmd[256];

	if (enabled) {
		snprintf(cmd, sizeof(cmd), "mkdir -p /mnt/sdcard/.minime/config/wifi");
		(void)system(cmd);
		f = fopen(WIFI_ENABLE_FILE, "w");
		if (f) {
			fputs("1\n", f);
			fclose(f);
		}
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

int WIFI_scan(void) {
	char cmd[256];

	if (!WIFI_enabled())
		return -1;
	snprintf(cmd, sizeof(cmd), WIFI_SCAN_CMD, wifi_interface());
	(void)system(cmd);
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

int WIFI_connect(const char *ssid, const char *passphrase) {
	char cmd[512];

	if (!ssid || !ssid[0])
		return -1;

	if (passphrase && passphrase[0]) {
		snprintf(cmd, sizeof(cmd), "iwctl --passphrase \"%s\" station %s connect \"%s\" >/dev/null 2>&1 &",
		         passphrase, wifi_interface(), ssid);
	} else {
		snprintf(cmd, sizeof(cmd), "iwctl station %s connect \"%s\" >/dev/null 2>&1 &",
		         wifi_interface(), ssid);
	}
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

	if (!ssid || !ssid[0])
		return -1;

	snprintf(cmd, sizeof(cmd), "iwctl known-networks \"%s\" forget >/dev/null 2>&1 &", ssid);
	return system(cmd) == 0 ? 0 : -1;
}
