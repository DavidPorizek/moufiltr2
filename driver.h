#pragma once
#include "ntddk.h"
#include <ntddmou.h>

//extern "C" DRIVER_INITIALIZE DriverEntry;

typedef struct _DEVICE_EXTENSION
{
	//
	// The top of the stack before this filter was added.  AKA the location
	// to which all IRPS should be directed.
	//
	PDEVICE_OBJECT  TopOfStack;

	LONG LastX;
	LONG LastY;

} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

//
// Forwards
//
NTSTATUS DriverEntry(
	IN PDRIVER_OBJECT  DriverObject,
	IN PUNICODE_STRING RegistryPath
);

NTSTATUS
MoufiltrAddDevice(
	IN PDRIVER_OBJECT   Driver,
	IN PDEVICE_OBJECT   PDO
);

NTSTATUS MoufiltrPnP(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP           Irp
);

NTSTATUS MoufiltrPower(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP           Irp
);

VOID
MoufiltrUnload(
	IN PDRIVER_OBJECT Driver
);

NTSTATUS MoufiltrRead(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp
);

NTSTATUS MoufiltrPostRead(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp,
	IN PVOID Context
);

NTSTATUS MoufiltrGeneral(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP           Irp
);




