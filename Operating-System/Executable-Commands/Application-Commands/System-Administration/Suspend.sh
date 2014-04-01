#!/bin/sh

PATH=/sbin:/usr/sbin:/usr/bin:/bin
ACPI_SUSPEND_STATE=hw.acpi.suspend_state
ACPI_SUPPORTED_STATES=hw.acpi.supported_sleep_state
APM_SUSPEND_DELAY=machdep.apm_suspend_delay

if sysctl $ACPI_SUSPEND_STATE >/dev/null 2>&1; then
	SUSPEND_STATE=`sysctl -n $ACPI_SUSPEND_STATE `
	SUPPORTED_STATES=`sysctl -n $ACPI_SUPPORTED_STATES `
	if echo $SUPPORTED_STATES | grep $SUSPEND_STATE >/dev/null; then
		exec acpiconf -s $SUSPEND_STATE
	else
		echo -n "Requested suspend state $SUSPEND_STATE "
		echo -n "is not supported.  "
		echo    "Supported states: $SUPPORTED_STATES"
	fi
elif sysctl $APM_SUSPEND_DELAY >/dev/null 2>&1; then
	exec apm -z
else
	echo "Error: no ACPI or APM suspend support found."
fi
exit 1
