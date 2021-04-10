/*
 * boot.c - a boot loader template (Assignment 1, ECE 6504)
 * Copyright 2021 Ruslan Nikolaev <rnikola@vt.edu>
 */

#include <Uefi.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/GraphicsOutput.h>
#include <Guid/FileInfo.h>

/* Use GUID names 'gEfi...' that are already declared in Protocol headers. */
EFI_GUID gEfiLoadedImageProtocolGuid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
EFI_GUID gEfiSimpleFileSystemProtocolGuid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
EFI_GUID gEfiGraphicsOutputProtocolGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
EFI_GUID gEfiFileInfoGuid = EFI_FILE_INFO_ID;

/* Keep these variables global. */
static EFI_HANDLE ImageHandle;
static EFI_SYSTEM_TABLE *SystemTable;
static EFI_BOOT_SERVICES *BootServices;

static VOID *AllocatePool(UINTN size, EFI_MEMORY_TYPE type)
{
	VOID *ptr;

	EFI_STATUS ret = BootServices->AllocatePool(type, size, &ptr);
	if (EFI_ERROR(ret))
		return NULL;
	return ptr;
}

static VOID *AllocatePages(UINTN pages, EFI_MEMORY_TYPE type)
{
	EFI_PHYSICAL_ADDRESS ptr = 0;

	EFI_STATUS ret = BootServices->AllocatePages(AllocateAnyPages, type, pages, &ptr);
	if (EFI_ERROR(ret))
		return NULL;
	
	return (VOID *)ptr;
}

static VOID FreePool(VOID *buf)
{
	BootServices->FreePool(buf);
}

static EFI_STATUS OpenFile(EFI_FILE_PROTOCOL **pvh, EFI_FILE_PROTOCOL **pfh, UINTN num)
{
	EFI_LOADED_IMAGE *li = NULL;
	EFI_FILE_IO_INTERFACE *fio = NULL;
	EFI_FILE_PROTOCOL *vh;
	EFI_STATUS efi_status;

	*pvh = NULL;
	*pfh = NULL;

	efi_status = BootServices->HandleProtocol(ImageHandle,
											  &gEfiLoadedImageProtocolGuid, (void **)&li);
	if (EFI_ERROR(efi_status))
	{
		SystemTable->ConOut->OutputString(SystemTable->ConOut,
										  L"Cannot get LoadedImage for BOOTx64.EFI\r\n");
										  BootServices->Stall(5 * 1000000);
		return efi_status;
	}

	efi_status = BootServices->HandleProtocol(li->DeviceHandle,
											  &gEfiSimpleFileSystemProtocolGuid, (void **)&fio);
	if (EFI_ERROR(efi_status))
	{
		SystemTable->ConOut->OutputString(SystemTable->ConOut,
										  L"Cannot get fio\r\n");
										  BootServices->Stall(5 * 1000000);
		return efi_status;
	}

	efi_status = fio->OpenVolume(fio, pvh);
	if (EFI_ERROR(efi_status))
	{
		SystemTable->ConOut->OutputString(SystemTable->ConOut,
										  L"Cannot get the volume handle!\r\n");
										  BootServices->Stall(5 * 1000000);
		return efi_status;
	}
	vh = *pvh;
	if(num==0){   // read kernel file
		efi_status = vh->Open(vh, pfh, L"\\EFI\\BOOT\\KERNEL",
						  EFI_FILE_MODE_READ, 0);
	}
	else if(num==1){  //read user file
		efi_status = vh->Open(vh, pfh, L"\\EFI\\BOOT\\USER",
						  EFI_FILE_MODE_READ, 0);
	}
	
	if (EFI_ERROR(efi_status))
	{
		SystemTable->ConOut->OutputString(SystemTable->ConOut,
										  L"Cannot get the file handle!\r\n");
										  BootServices->Stall(5 * 1000000);
		return efi_status;
	}

	return EFI_SUCCESS;
}

static void CloseFile(EFI_FILE_PROTOCOL *vh, EFI_FILE_PROTOCOL *fh)
{
	vh->Close(vh);
	fh->Close(fh);
}

static UINT32 *SetGraphicsMode(UINT32 width, UINT32 height)
{
	EFI_GRAPHICS_OUTPUT_PROTOCOL *graphics;
	EFI_STATUS efi_status;
	UINT32 mode;

	efi_status = BootServices->LocateProtocol(&gEfiGraphicsOutputProtocolGuid,
											  NULL, (VOID **)&graphics);
	if (EFI_ERROR(efi_status))
	{
		SystemTable->ConOut->OutputString(SystemTable->ConOut,
										  L"Cannot get the GOP handle!\r\n");
										  BootServices->Stall(5 * 1000000);
		return NULL;
	}

	if (!graphics->Mode || graphics->Mode->MaxMode == 0)
	{
		SystemTable->ConOut->OutputString(SystemTable->ConOut,
										  L"Incorrect GOP mode information!\r\n");
										  BootServices->Stall(5 * 1000000);
		return NULL;
	}

	for (mode = 0; mode < graphics->Mode->MaxMode; mode++)
	{
		EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info;
		UINTN size;

		// TODO: Locate BlueGreenRedReserved, aka BGRA (8-bit per color)
		// Resolution width x height (800x600 in our code)

		// Activate (set) this graphics mode
		efi_status = graphics->QueryMode(graphics, mode, &size, &info);

		if (EFI_ERROR(efi_status))
		{
			SystemTable->ConOut->OutputString(SystemTable->ConOut,
											  L"ERROR: Bad response from QueryMode\r\n");
			BootServices->Stall(5 * 1000000);
			continue;
		}

		switch (info->PixelFormat)
		{

			case PixelBlueGreenRedReserved8BitPerColor:
				if(info->HorizontalResolution==width && info->VerticalResolution==height){
					graphics->SetMode(graphics, mode);
				}
				else{
					continue;
				}
				
				break;
			default:
			continue;
		}

		
		// Return the frame buffer base address
		return (UINT32 *)graphics->Mode->FrameBufferBase;
	}
	return NULL;
}

static VOID *LoadFile(EFI_FILE_PROTOCOL *fh, UINTN* size)
{

	EFI_STATUS efi_status;
	EFI_FILE_INFO *file_info = NULL;
	VOID *buffer = NULL;

	//Read the file info size.
	efi_status = fh->GetInfo(fh, &gEfiFileInfoGuid, size, NULL);
	if (efi_status == EFI_BUFFER_TOO_SMALL)
	{
		file_info = AllocatePool(*size, EfiBootServicesData);
	}
	else
	{
		SystemTable->ConOut->OutputString(SystemTable->ConOut,
										  L"Cannot get file size.\r\n");
		BootServices->Stall(5 * 1000000); 
	}

	efi_status = fh->GetInfo(fh, &gEfiFileInfoGuid, size, file_info);
	if (EFI_ERROR(efi_status))
	{
		SystemTable->ConOut->OutputString(SystemTable->ConOut,
										  L"Cannot get the file info.\r\n");
		BootServices->Stall(5 * 1000000); 
	}
	else
	{
		*size = file_info->FileSize;
	}
	FreePool(file_info);

	//Now that we have the actual file size, convert it to pages and allocate a buffer using AllocatePages()
	UINTN pages = EFI_SIZE_TO_PAGES(*size);
	buffer = AllocatePages(pages, EfiLoaderCode);

	//Read the file
	efi_status = fh->Read(fh, size, buffer);
	if(EFI_ERROR(efi_status))
	{
		SystemTable->ConOut->OutputString(SystemTable->ConOut,
										  L"Error while reading file.\r\n");
		BootServices->Stall(5 * 1000000); // 5 seconds
		
	}
	return buffer;
}

static VOID setExitBootServices()
{
	EFI_STATUS efi_status;
	UINTN mapSize = 0, mapKey, descriptorSize;
    EFI_MEMORY_DESCRIPTOR *memoryMap = NULL;
    UINT32 descriptorVersion;

	//Memory map

	do{
		efi_status = BootServices->GetMemoryMap(&mapSize, memoryMap, &mapKey, &descriptorSize, &descriptorVersion);
		if(efi_status==EFI_BUFFER_TOO_SMALL){
			UINTN pages = EFI_SIZE_TO_PAGES(mapSize+1);
			memoryMap = AllocatePages(pages, EfiLoaderData);
			efi_status = BootServices->GetMemoryMap(&mapSize, memoryMap, &mapKey, &descriptorSize, &descriptorVersion);
		
		}
		
	} while(efi_status!=EFI_SUCCESS);

	//Exit boot services

	while(1){
		efi_status = BootServices->ExitBootServices(ImageHandle, mapKey);
		if (efi_status == EFI_SUCCESS) {
			break; 
		}
		if (efi_status != EFI_INVALID_PARAMETER) {
			SystemTable->ConOut->OutputString(SystemTable->ConOut,
										  L"Exit Boot Services error!\r\n");
			BootServices->Stall(5 * 1000000);
			break;
		}

	}
	
}


/* Use System V ABI rather than EFI/Microsoft ABI. */
typedef void (*kernel_entry_t)(void *, unsigned int *, int, void *, void *, int) __attribute__((sysv_abi));


EFI_STATUS EFIAPI
efi_main(EFI_HANDLE imageHandle, EFI_SYSTEM_TABLE *systemTable)
{
	
	EFI_FILE_PROTOCOL *vh, *fh, *vh2, *fh2;
	EFI_STATUS efi_status;
	UINT32 *fb;

	ImageHandle = imageHandle;
	SystemTable = systemTable;
	BootServices = systemTable->BootServices;


/*

Read Kernel File

*/

	// Get the volume handle and file handle of the kernel file
	efi_status = OpenFile(&vh, &fh,0);
	if (EFI_ERROR(efi_status))
	{
		SystemTable->ConOut->OutputString(SystemTable->ConOut,
										  L"Error getting file and volume handles of kernel file!\r\n");
		BootServices->Stall(5 * 1000000); // 5 seconds
		return efi_status;
	}

	
	// Load the kernel using the file handle
	UINTN kernel_size=0;
	VOID *kernel_buffer  = LoadFile(fh, &kernel_size);
	 
	//Close the volume and file handles
	CloseFile(vh, fh);


/*

Read User File

*/


	//Allocate pages 2054 + 1 page (for kernel stack)
	VOID *kernel_addr = AllocatePages(2055, EfiLoaderData);
	kernel_addr += 0x1000;
	
	
	// Get the volume handle and file handle of the user app
	efi_status = OpenFile(&vh2, &fh2, 1);
	if (EFI_ERROR(efi_status))
	{
		SystemTable->ConOut->OutputString(SystemTable->ConOut,
										  L"Error getting file and volume handles of user app!\r\n");
		BootServices->Stall(5 * 1000000); // 5 seconds
		return efi_status;
	}


	// Load the user app using the file handle
	UINTN user_size=0;
 	VOID *user_buffer = LoadFile(fh2,&user_size);
	UINTN user_pages = EFI_SIZE_TO_PAGES(user_size);

	//Close the volume and file handles
		CloseFile(vh2, fh2);
/*
	Allocate pages 4 (for new user page table) + 1 page (for USER stack)
	+ 1 page (for tss rsp[0]) + 1 page (for lazy load)
	+ 1 page (for tss)
	+ 1 page (for TLS)
*/
	VOID *user_addr = AllocatePages(9, EfiLoaderData);
	user_addr += 0x1000;

/*

Frame Buffer

*/

	//Get the frame buffer base address
	fb = SetGraphicsMode(800, 600);

/*

Exit Boot Services

*/

	//Memory Map and ExitBootServices()
	setExitBootServices();
	

	// kernel's _start() is at base #0 (pure binary format)
	// cast the function pointer appropriately and call the function
	kernel_entry_t func = (kernel_entry_t)kernel_buffer;
	func(  kernel_addr, fb, 800, user_addr, user_buffer, user_pages );

	return EFI_SUCCESS;
}
