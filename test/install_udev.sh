TMP_FILE=99-samsung-device.rules
echo "# Add a udev rules when you want to develop a Tizen application with your device." >> $TMP_FILE
echo "# Use this format to add each vendor to the file:" >> $TMP_FILE
echo "# SUBSYSTEM==\"usb\", ATTR{idVendor}==\"04e8\", ATTRS{idProduct}==\"6864\", MODE=\"0666\", GROUP=\"plugdev\"" >> $TMP_FILE
echo "# In the example, the vendor ID is for Samsung manufacture. The mode specifies read/write permissions, and group defines which Unix group owns the device node." >> $TMP_FILE
echo "#" >> $TMP_FILE
echo "# Contact : Kangho Kim <kh5325.kim@samsung.com>, Yoonki Park<yoonki.park@samsung.com>, Ho Namkoong <ho.namkoong@samsung.com>" >> $TMP_FILE
echo "# See also udev(7) for an overview of rule syntax." >> $TMP_FILE
echo "" >> $TMP_FILE
echo "# Samsung" >> $TMP_FILE
echo "SUBSYSTEMS==\"usb\", ATTRS{idVendor}==\"04e8\", ATTRS{idProduct}==\"6864\", MODE=\"0666\", GROUP=\"plugdev\"" >> $TMP_FILE
echo "SUBSYSTEMS==\"usb\", ATTRS{idVendor}==\"04e8\", ATTRS{idProduct}==\"6863\", MODE=\"0666\", GROUP=\"plugdev\"" >> $TMP_FILE
chmod +x $TMP_FILE
#gksudo mv ${TMP_FILE} /etc/udev/rules.d/
