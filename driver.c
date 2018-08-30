#include "driver.h"

#define MAX_ARRAY_SIZE 255

//GLOBAL

MOUSE_INPUT_DATA g_MouseDataArray[MAX_ARRAY_SIZE];
LONG g_MouseDataArrayIndex;


NTSTATUS DriverEntry(
	IN PDRIVER_OBJECT  DriverObject,
	IN PUNICODE_STRING RegistryPath
)
{
	UNREFERENCED_PARAMETER(RegistryPath);

	DbgPrint(("Ctrl2cap.SYS: entering DriverEntry\n"));

	for (int i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++) {

		DriverObject->MajorFunction[i] = MoufiltrGeneral;
	}

	RtlZeroMemory(g_MouseDataArray, MAX_ARRAY_SIZE * sizeof(MOUSE_INPUT_DATA));
	g_MouseDataArrayIndex = 0;

	DriverObject->MajorFunction[IRP_MJ_READ] = MoufiltrRead;
	//DriverObject->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = MoufiltrInternal;

	//
	// Power IRPs are the only ones we have to handle specially under
	// Win2k since they require the special PoCallDriver and 
	// PoStartNextPowerIrp function calls.
	//
	DriverObject->MajorFunction[IRP_MJ_POWER] = MoufiltrPower;

	//
	// The only reason we need to handle PnP IRPs is to know when
	// a device we've attached to disappears (is removed).
	//
	DriverObject->MajorFunction[IRP_MJ_PNP] = MoufiltrPnP;

	//
	// Under Win2K we get told about the presence of keyboard 
	// devices through our AddDevice entry point.
	//
	DriverObject->DriverUnload = MoufiltrUnload;
	DriverObject->DriverExtension->AddDevice = MoufiltrAddDevice;

	return STATUS_SUCCESS;
}


VOID
MoufiltrUnload(
	IN PDRIVER_OBJECT Driver
)
{
	UNREFERENCED_PARAMETER(Driver);

	ASSERT(NULL == Driver->DeviceObject);
}


// We have to handle PnP IRPs so that we detach from target
// devices when appropriate.
NTSTATUS 
MoufiltrPnP(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP           Irp
)
{
	PDEVICE_EXTENSION           devExt;
	PIO_STACK_LOCATION          irpStack;
	NTSTATUS                    status = STATUS_SUCCESS;

	devExt = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
	irpStack = IoGetCurrentIrpStackLocation(Irp);

	switch (irpStack->MinorFunction) {
	case IRP_MN_REMOVE_DEVICE:

		//
		// Detach from the target device after passing the IRP
		// down the devnode stack.
		//
		IoSkipCurrentIrpStackLocation(Irp);
		IoCallDriver(devExt->TopOfStack, Irp);

		IoDetachDevice(devExt->TopOfStack);
		IoDeleteDevice(DeviceObject);

		status = STATUS_SUCCESS;
		break;

	case IRP_MN_SURPRISE_REMOVAL:

		//
		// Same as a remove device, but don't call IoDetach or IoDeleteDevice.
		//
		IoSkipCurrentIrpStackLocation(Irp);
		status = IoCallDriver(devExt->TopOfStack, Irp);
		break;

	case IRP_MN_START_DEVICE:
	case IRP_MN_QUERY_REMOVE_DEVICE:
	case IRP_MN_QUERY_STOP_DEVICE:
	case IRP_MN_CANCEL_REMOVE_DEVICE:
	case IRP_MN_CANCEL_STOP_DEVICE:
	case IRP_MN_FILTER_RESOURCE_REQUIREMENTS:
	case IRP_MN_STOP_DEVICE:
	case IRP_MN_QUERY_DEVICE_RELATIONS:
	case IRP_MN_QUERY_INTERFACE:
	case IRP_MN_QUERY_CAPABILITIES:
	case IRP_MN_QUERY_DEVICE_TEXT:
	case IRP_MN_QUERY_RESOURCES:
	case IRP_MN_QUERY_RESOURCE_REQUIREMENTS:
	case IRP_MN_READ_CONFIG:
	case IRP_MN_WRITE_CONFIG:
	case IRP_MN_EJECT:
	case IRP_MN_SET_LOCK:
	case IRP_MN_QUERY_ID:
	case IRP_MN_QUERY_PNP_DEVICE_STATE:
	default:
		//
		// Pass these through untouched
		//
		IoSkipCurrentIrpStackLocation(Irp);
		status = IoCallDriver(devExt->TopOfStack, Irp);
		break;
	}

	return status;
}


// We have to handle Power IRPs specially.
NTSTATUS 
MoufiltrPower(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP           Irp
)
{
	PDEVICE_EXTENSION   devExt;

	devExt = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;

	//
	// Let the next power IRP out of the gate
	// 
	PoStartNextPowerIrp(Irp);

	//
	// Pass this power IRP to the keyboard class driver
	//
	IoSkipCurrentIrpStackLocation(Irp);

	return PoCallDriver(devExt->TopOfStack, Irp);
}


NTSTATUS 
MoufiltrAddDevice(
	IN PDRIVER_OBJECT   DriverObject,
	IN PDEVICE_OBJECT   PDO
)
{
	NTSTATUS status;
	PDEVICE_EXTENSION extension;
	PDEVICE_OBJECT device;

	status = IoCreateDevice(DriverObject, sizeof(DEVICE_EXTENSION), NULL, FILE_DEVICE_MOUSE, 0,
		FALSE, &device);

	if (!NT_SUCCESS(status)) {

		return (status);
	}

	RtlZeroMemory(device->DeviceExtension, sizeof(DEVICE_EXTENSION));

	extension = (PDEVICE_EXTENSION)device->DeviceExtension;
	extension->TopOfStack = IoAttachDeviceToDeviceStack(device, PDO);

	ASSERT(extension->TopOfStack);

	device->Flags |= (DO_BUFFERED_IO | DO_POWER_PAGABLE);
	device->Flags &= ~DO_DEVICE_INITIALIZING;
	return status;

}

NTSTATUS 
MoufiltrPostRead(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp,
	IN PVOID Context
)
{
	PIO_STACK_LOCATION IrpSp;
	PMOUSE_INPUT_DATA KeyData;
	//int numKeys;

	UNREFERENCED_PARAMETER(Context);
	UNREFERENCED_PARAMETER(DeviceObject);

	IrpSp = IoGetCurrentIrpStackLocation(Irp);
	if (NT_SUCCESS(Irp->IoStatus.Status)) {

		KeyData = (PMOUSE_INPUT_DATA)Irp->AssociatedIrp.SystemBuffer;
	}

	if (Irp->PendingReturned) {

		IoMarkIrpPending(Irp);
	}
	return Irp->IoStatus.Status;
}


NTSTATUS 
MoufiltrRead(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp)
{
	PDEVICE_EXTENSION devExt;
	//PIO_STACK_LOCATION currentIrpStack;
	//PIO_STACK_LOCATION nextIrpStack;
	//PMOUSE_INPUT_DATA MouseData;

	//LONG difX = 0;
	//LONG difY = 0;

	//
	// Gather our variables.
	//
	devExt = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
	//currentIrpStack = IoGetCurrentIrpStackLocation(Irp);


	//
	// Return the results of the call to the keyboard class driver.
	//
	IoCopyCurrentIrpStackLocationToNext(Irp);

	return IoCallDriver(devExt->TopOfStack, Irp);
}


NTSTATUS 
MoufiltrGeneral(
	IN PDEVICE_OBJECT DeviceObject, 
	IN PIRP Irp
)
{
	IoSkipCurrentIrpStackLocation(Irp);

	return IoCallDriver(((PDEVICE_EXTENSION)DeviceObject->DeviceExtension)->TopOfStack, Irp);
}