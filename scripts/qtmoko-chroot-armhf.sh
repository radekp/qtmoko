# Script for debian wheezy/armhf chroot with all dependencies for QtMoko
# For the first time it installs the chroot later it just enters it

# set -o verbose

# We will use ext4 image for chroot
if [ ! -e ../qtmoko-chroot.img ]
then
    echo "Creating chroot-build ext4 image"
    dd if=/dev/zero of=../qtmoko-chroot.img bs=1024 count=3145728
    mkfs.ext4 ../qtmoko-chroot.img
fi

# Directory for chroot mount
if [ ! -d ../qtmoko-chroot ]
then
    mkdir -p ../qtmoko-chroot
fi    

# Mount the image if not mounted yet
if [ ! -d ../qtmoko-chroot/bin ]
then
    echo "Mounting image"
    mount -o loop ../qtmoko-chroot.img ../qtmoko-chroot
fi    

# Install squeeze rootfs if not installed yet
if [ ! -d ../qtmoko-chroot/bin ]
then
    if [ ! -f /usr/bin/cdebootstrap ]
    then
	echo "Installing cdebootstrap package"
	apt-get install cdebootstrap
    fi

    echo "Installing chroot packages"
    until cdebootstrap --flavour=minimal --include=build-essential,git,openssh-client,ccache,locales,procps,psmisc,libxext-dev,libasound2-dev,libdbus-1-dev,libssl-dev,libts-dev,libbluetooth-dev,libxtst-dev,libpng12-dev,libjpeg8-dev,libv4l-dev,libspeexdsp-dev,libglib2.0-dev,libsqlite3-dev,quilt,libgstreamer0.10-dev,libgstreamer-plugins-base0.10-dev,libpulse-dev,libgps-dev,wget,curl wheezy ../qtmoko-chroot http://cdn.debian.net/debian/; do
	:
    done
fi

if [ ! -d ../qtmoko-chroot/proc/bus ]
then
    echo "Mounting chroot dirs"
    mount -t proc none ../qtmoko-chroot/proc
    mount -t sysfs none ../qtmoko-chroot/sys
    mount -o bind /dev ../qtmoko-chroot/dev

    if [ ! -d ../qtmoko-chroot/root/qte ]
    then
        mkdir -p ../qtmoko-chroot/root/qte
    fi
    mount -o bind .. ../qtmoko-chroot/root/qte

    if [ ! -d ../qtmoko-chroot/var/lib/dbus ]
    then
        mkdir -p ../qtmoko-chroot/var/lib/dbus
    fi
    mount -o bind .. ../qtmoko-chroot/var/lib/dbus

    if [ ! -d ../qtmoko-chroot/var/lib/dbus ]
    then
        mkdir -p ../qtmoko-chroot/var/lib/dbus
    fi
    mount -o bind /var/lib/dbus ../qtmoko-chroot/var/lib/dbus
    mount -o bind /var/run/ ../qtmoko-chroot/var/run
fi

if [ ! -e ../qtmoko-chroot/usr/bin/arm-linux-gnueabihf-gcc-4.7 ]
then
    cat > ../qtmoko-chroot/finish_chroot_install.sh <<__END__
#!/bin/bash
echo "Installing emdebian toolchain"
apt-get update
apt-get install emdebian-archive-keyring
echo "deb http://cdn.debian.net/debian wheezy main contrib non-free" > /etc/apt/sources.list
echo "deb http://www.emdebian.org/debian unstable main" >> /etc/apt/sources.list
apt-get update
apt-get install g++-4.7-arm-linux-gnueabihf

echo "Installing xapt and ARM qtmoko dependencies"
apt-get install xapt
xapt -a armhf -m libxext-dev libasound2-dev libdbus-1-dev libssl-dev libts-dev libbluetooth-dev libxtst-dev libpng12-dev libjpeg8-dev libv4l-dev libspeexdsp-dev libglib2.0-dev libsqlite3-dev libgstreamer0.10-dev libgstreamer-plugins-base0.10-dev libvorbis-dev libpulse-dev libssl-dev libgps-dev
pushd /usr/arm-linux-gnueabihf/lib/
ln -s ld-linux-armhf.so.3 ld-linux.so.3
popd

# For some reason libpulsecommon is not fetched by xapt, so do it manually
cd /tmp
curl -O http://ftp.de.debian.org/debian/pool/main/p/pulseaudio/libpulse0_2.0-6.1_armhf.deb
dpkg -x libpulse0_2.0-6.1_armhf.deb .
cp usr/lib/arm-linux-gnueabihf/pulseaudio/libpulsecommon-2.0.so /usr/arm-linux-gnueabihf/lib/


echo "export PATH=/usr/lib/ccache:\$PATH" >> /root/.bashrc
echo "PS1='qtmoko-chroot:\w\\\$ '" >> /root/.bashrc
echo "export LANG=C" >> /root/.bashrc
echo "export LC_ALL=C" >> /root/.bashrc

echo "+-----------------------------------------------------------------+"
echo "| Success! You can now build QtMoko like this:                    |"
echo "|                                                                 |"
echo "| mkdir -p /root/qte/build                                        |"
echo "| cd /root/qte/build                                              |"
echo "| ../qtmoko/configure -device gta04                               |"
echo "| make                                                            |"
echo "| export LD_LIBRARY_PATH=/root/qte/build/qtopiacore/host/lib/     |"
echo "| make install                                                    |"
echo "|                                                                 |"
echo "+-----------------------------------------------------------------+"

__END__

    chmod +x ../qtmoko-chroot/finish_chroot_install.sh
    chroot ../qtmoko-chroot /finish_chroot_install.sh
    rm -f ../qtmoko-chroot/finish_chroot_install.sh
fi

chroot ../qtmoko-chroot /bin/bash

