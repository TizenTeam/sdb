#!/bin/sh

if [ $# -ne 1 ];then
        echo "Usage: $0 [device]"
        exit 1
fi

UBUNTU_VER=`awk 'BEGIN {FS="="}; /DISTRIB_RELEASE.*/ {print $2}' /etc/lsb-release`
IPADDR=192.168.129.1

case $UBUNTU_VER in
	8.04)
		NETMGR="/etc/dbus-1/event.d/25NetworkManager"
		echo "8.04"
		;;
	*)
		NETMGR="service network-manager"
		echo "${UBUNTU_VER}"
		;;
esac
			
getDevState() {
	DEV_CONF_STATE=-1
	until [ $DEV_CONF_STATE -eq 1 ]; do
		DEV_CONF_STATE=`ifconfig | awk -v intRecNum=-1 '/samsung_device/ {intRecNum=NR;} /inet addr/ {if(intRecNum==NR-1) {intRecNum=-2; exit}} END {if(intRecNum==-1) print 0; else if(intRecNum==-2) print 2; else if(intRecNum > 0) print 1;}'`
		echo $DEV_CONF_STATE
		sleep 1
	done
}

case "$1" in
	stop)
		echo 0 > /var/run/udev-usbnet
		echo 0 > /proc/sys/net/ipv4/ip_forward
		iptables -t nat -D POSTROUTING -s 192.168.129.3 -j MASQUERADE
		if [ $UBUNTU_VER != "8.04" ] && [ $UBUNTU_VER != "8.10" ] && [ $UBUNTU_VER != "9.04" ]; then
			${NETMGR} start
		fi
		exit 0
		;;
	*)
		if [ $UBUNTU_VER != "8.10" ] && [ $UBUNTU_VER != "9.04" ] &&  [ $UBUNTU_VER != "8.04" ]; then
			${NETMGR} stop
		else
			getDevState
		fi
		/sbin/ifconfig $1 $IPADDR netmask 255.255.255.0 broadcast 192.168.129.255 up 
		if [ $UBUNTU_VER == "8.10" ] || [ $UBUNTU_VER == "9.04" ] ||  [ $UBUNTU_VER == "8.04" ]; then
			getDevState
			/sbin/ifconfig $1 $IPADDR netmask 255.255.255.0 broadcast 192.168.129.255 up 
		fi
		echo 1 > /proc/sys/net/ipv4/ip_forward
		iptables -t nat -A POSTROUTING -s 192.168.129.3 -j MASQUERADE
		echo 1 > /var/run/udev-usbnet

		exit 0
		;;
esac

