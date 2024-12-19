Originally from [Reddit](https://www.reddit.com/r/synology/comments/etz32q/instructions_on_how_to_install_mergerfs_on_a/). Copied and edited with permission.

A different version to overcome some problems with the method below, can be [found here](https://web.archive.org/web/20221205205446/https://daniellemarco.nl/wp/2022/01/01/adding-mergerfs-to-your-synology/)

Install Entware

1. SSH into your NAS and switch to the root user:

```
sudo su
```

2. Create a folder on your hdd (outside rootfs):

```
mkdir -p /volume1/@Entware/opt
```

2. Remove `/opt` and mount optware folder.

Make sure that `/opt` folder is empty (Optware is not installed), we will remove the `/opt` folder with its contents at this step.

```
rm -rf /opt
mkdir /opt
mount -o bind "/volume1/@Entware/opt" /opt
```

3. Run install script depending on the processor. Use command `uname -m` to find out. Then run the corresponding command.

#### armv8 (aarch64) - Realtek RTD129x

```
wget -O - http://bin.entware.net/aarch64-k3.10/installer/generic.sh | /bin/sh
```

#### armv5

```
wget -O - http://bin.entware.net/armv5sf-k3.2/installer/generic.sh | /bin/sh
```

#### armv7

```
wget -O - http://bin.entware.net/armv7sf-k3.2/installer/generic.sh | /bin/sh
```

#### x64

```
wget -O - http://bin.entware.net/x64-k3.2/installer/generic.sh | /bin/sh
```

4. Create an Autostart Task On Synology

Create a triggered user-defined task in Task Scheduler.

- Go to: DSM > Control Panel > Task Scheduler
- Create > Triggered Task > User Defined Script
  - General
  - Task: Entware
  - User: root
  - Event: Boot-up
  - Pretask: none
- Task Settings
  - Run Command: Paste the below script in.

```
#!/bin/sh

# Mount/Start Entware
mkdir -p /opt
mount -o bind "/volume1/@Entware/opt" /opt
/opt/etc/init.d/rc.unslung start

# Add Entware Profile in Global Profile
if grep  -qF  '/opt/etc/profile' /etc/profile; then
	echo "Confirmed: Entware Profile in Global Profile"
else
	echo "Adding: Entware Profile in Global Profile"
cat >> /etc/profile <<"EOF"

# Load Entware Profile
[ -r "/opt/etc/profile" ] && . /opt/etc/profile
EOF
fi

# Update Entware List
/opt/bin/opkg update
```

5. Reboot your NAS.

6. SSH back into your NAS

7. Install mergerfs by the following command.

```
sudo opkg install mergerfs
```

9. Make sure it's installed by running the following command. Mergerfs binary is expected to be listed there.

```
sudo ls /volume1/@Entware/opt/bin
```

This should print the usage helper of mergerfs.

```
mergerfs --help
```

10. If you want the latest build of mergerfs, you can download the `mergerfs-static-linux_$ARCH.tar.gz` from [Github releases page](https://github.com/trapexit/mergerfs/releases/latest), remember to replace `$ARCH` with your architecture, e.g. what `uname -m` tells you.

Extract the `.tar.gz` archive and use its content to update the `mergerfs` and `mergerfs-fusermount` binaries in `/opt/bin/`

11. Configure mergerfs. Note: Change the file paths to your setup.

_MY CONFIG IS (Don't know if it is the perfect setting, but works in my testing) _

```
mergerfs -o rw,use_ino,allow_other,func.getattr=newest,category.action=all,category.create=ff,dropcacheonclose=true /volume1/Media/TempMedia:/volume1/Media/GMedia /volume1/Media/FinalMedia
```

If mergerfs complains about existing files because the destination already has the Synology `@eaDir` directory, you can use the option `nonempty`.

12. Create an Autostart Task On Synology for Mergerfs

- Go to: DSM > Control Panel > Task Scheduler
- Create > Triggered Task > User Defined Script
  - General
  - Task: Mergerfs
  - User: root
  - Event: Boot-up
  - Pretask: Entware
- Task Settings
  - Run Command: Paste the below script in.

```
#!/bin/sh

/opt/bin/mergerfs -o rw,use_ino,allow_other,func.getattr=newest,category.action=all,category.create=ff,dropcacheonclose=true /volume1/Media/TempMedia:/volume1/Media/GMedia /volume1/Media/FinalMedia

```

13. Profit
