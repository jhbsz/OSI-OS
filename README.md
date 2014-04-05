_**OSI OS Development Repository**_
Repository Root Directory
--------------------------

**Github URL**
* [JustinMuniz/OSI-OS](https://github.com/JustinMuniz/OSI-OS)


**Project Description**
* Operating system for general purpose computer systems.


**Project Goals**
* Reform FreeBSD to:
 * have smaller code repository size,
 * utilize a new method of storing external source code files
  * such as documentation, configuration, and compilation information,
 * incorporate a more meaningful file naming scheme,
 * be simpler to customize and deploy,
 * exclude software licensed with the GPL,
 * 


**Directory Structure** (/)
* Applications
* Operating-System
* User-Data


**Reminders**
* Migrate executable commands to locations with file names to provide understanding
 * /bin
 * /sbin
 * /usr.bin
 * /usr.sbin
* Remember to update shell paths
* Move /kerberos5 and /secure to /O-S/Export-Controlled/
* Create /Compatibility with freebsd conversion
* Rebuild documentation
* Create GUI for compilation and kernel configure
* Delete OSI-OS / cddl / contrib / dtracetoolkit / Examples / Readme
* Delete OSI-OS / cddl / contrib / dtracetoolkit / Man / man1m
