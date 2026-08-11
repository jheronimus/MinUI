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
		if (strstr(line, "Connected network")) {
			char *value = strchr(line, ':');
			size_t len;

			if (!value)
				continue;
			value++;
			while (*value == ' ' || *value == '\t')
				value++;
			len = strlen(value);
			while (len > 0 && (value[len - 1] == '\n' || value[len - 1] == '\r')) {
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

	if (!f)
		return 0;
	while (fgets(line, sizeof(line), f)) {
		char *val;

		if (strncmp(line, "SSID=", 5) != 0)
			continue;
		val = line + 5;
		{
			size_t len = strlen(val);
			while (len > 0 && (val[len - 1] == '\n' || val[len - 1] == '\r')) {
				val[len - 1] = '\0';
				len--;
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

// Parse `iwctl station <if> get-networks` output. Rows are right-anchored:
//   <name...> <security> <signal>
// The signal field is always a number, so parse from the right.
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
		char *tokens[32];
		int n = 0;
		char *saveptr = NULL;
		char *tok;
		char *last;
		char *sec;
		int i;
		int signal;

		trim(line);
		if (!line[0])
			continue;

		tok = strtok_r(line, " \t", &saveptr);
		while (tok && n < 32) {
			tokens[n++] = tok;
			tok = strtok_r(NULL, " \t", &saveptr);
		}
		if (n < 3)
			continue;

		// signal is the last field and must be numeric
		signal = (int)strtol(tokens[n - 1], &last, 10);
		if (*last != '\0' || tokens[n - 1][0] == '-')
			continue;

		sec = tokens[n - 2];
		{
			WifiNetwork *net = &networks[network_count];

			net->ssid[0] = '\0';
			for (i = 0; i < n - 2; i++) {
				if (i > 0)
					strncat(net->ssid, " ", sizeof(net->ssid) - strlen(net->ssid) - 1);
				strncat(net->ssid, tokens[i], sizeof(net->ssid) - strlen(net->ssid) - 1);
			}
			if (!net->ssid[0])
				continue;

			if (signal > 100)
				signal = 100;
			if (signal < 0)
				signal = 0;
			net->signal = signal;
			net->security = (strstr(sec, "open") || !strcasecmp(sec, "open")) ? WIFI_SECURITY_OPEN
			                                                                  : WIFI_SECURITY_WPA;
			net->known = is_ssid_known(net->ssid);
			net->connected = 0;
			network_count++;
		}
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
	return wifi_enabled;
}

int WIFI_setEnabled(int enabled) {
	int rc;

	if (enabled) {
		FILE *f;
		char cmd[256];

		// touch the gate so wifi persists across reboots
		snprintf(cmd, sizeof(cmd), "mkdir -p /mnt/sdcard/.minime/config/wifi");
		(void)system(cmd);
		f = fopen(WIFI_ENABLE_FILE, "w");
		if (f) {
			fputs("1\n", f);
			fclose(f);
		}
		rc = system(WIFI_SERVICE " start");
		if (rc != 0 && !is_wifi_interface_present()) {
			(void)system(WIFI_SERVICE " stop");
			wifi_enabled = 0;
			return -1;
		}
		wifi_enabled = 1;
	} else {
		unlink(WIFI_ENABLE_FILE);
		rc = system(WIFI_SERVICE " stop");
		if (rc != 0)
			return -1;
		wifi_enabled = 0;
	}
	return 0;
}

int WIFI_scan(void) {
	char cmd[256];

	if (!wifi_enabled)
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
