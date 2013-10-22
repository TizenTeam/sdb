#!/bin/bash -ex
SDB_PATH=tools/sdb
${INSTALLED_PATH}/${SDB_PATH} kill-server
rm -rf ${INSTALLED_PATH}/${SDB_PATH}

UDEV_RULE=/etc/udev/rules.d/99-samsung-device.rules

if [ ! -f ${UDEV_RULE} ]; then
    exit 0
fi
## remove udev rule file
if [ -z "$TSUDO" ]; then
    if [ -f /usr/bin/gksudo ]
        then gksudo rm -rf ${UDEV_RULE}
        else if [ -f /usr/bin/sudo ]
            then sudo rm -rf ${UDEV_RULE}
        fi
    fi
    exit 0
else
    $TSUDO -m "Enter your password to uninstall sdb." rm -rf ${UDEV_RULE}
    exit 0
fi

exit 0
