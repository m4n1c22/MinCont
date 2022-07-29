# MinCont
This is a minimalistic container manager.

## Steps to run the container manager

- Pull the repository.
- Run the ``` make``` command which by default would build the rootfs and manager executable. It will also run the container within the manager.

## Features supported

- Container environment to run various applications.
- Isolated namespaces for IPC, netlink, proc.
- Unique Hostname 

## Not yet supported

- Run /sbin/init within the container and extract boot logs.
- Perform DHCP over eth0 and fetch an ip address.

Git repo: https://github.com/m4n1c22/MinCont


