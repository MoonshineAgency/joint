#!/bin/bash

[ $# -ne 2 ] && echo "Usage: script.sh SSID password" && exit

curl -X POST -i 'http://192.168.1.1/api/settings' --data '{
	"system": {
		"name": "Growtent-dev",
		"failsafe":     true,
		"safe_mode":    false,
		"timezone":     "UTC-05"
	},
	"mqtt": {
		"uri":  "mqtt://192.168.88.200:1883",
		"username":     "node",
		"password":     "node"
	},
	"wifi": {
		"ip": {
			"dhcp": true,
			"ip": "192.168.1.1",
			"netmask": "255.255.255.0",
			"gateway": "192.168.1.1",
			"dns": "192.168.1.1"
		},
		"ap": {
			"ssid": "Joint",
			"channel": 6,
			"password": ""
		},
		"sta": {
			"ssid": "' "$1" '",
			"password": "' "$2" '"
		}
	}
}'

curl -X GET -i 'http://192.168.1.1/api/reboot'
