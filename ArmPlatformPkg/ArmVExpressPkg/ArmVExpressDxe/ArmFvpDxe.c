/** @file

  Copyright (c) 2013-2015, ARM Ltd. All rights reserved.<BR>

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "ArmVExpressInternal.h"

#include <Library/VirtioMmioDeviceLib.h>
#include <Library/ArmShellCmdLib.h>
#include <Library/MemoryAllocationLib.h>

#define ARM_FVP_BASE_VIRTIO_BLOCK_BASE    0x1c130000
STATIC CONST CHAR16 *mFdtFallbackName = L"fdt.dtb";

#pragma pack(1)
typedef struct {
  VENDOR_DEVICE_PATH                  Vendor;
  EFI_DEVICE_PATH_PROTOCOL            End;
} VIRTIO_BLK_DEVICE_PATH;
#pragma pack()

VIRTIO_BLK_DEVICE_PATH mVirtioBlockDevicePath =
{
  {
    {
      HARDWARE_DEVICE_PATH,
      HW_VENDOR_DP,
      {
        (UINT8)( sizeof(VENDOR_DEVICE_PATH) ),
        (UINT8)((sizeof(VENDOR_DEVICE_PATH)) >> 8)
      }
    },
    EFI_CALLER_ID_GUID,
  },
  {
    END_DEVICE_PATH_TYPE,
    END_ENTIRE_DEVICE_PATH_SUBTYPE,
    {
      sizeof (EFI_DEVICE_PATH_PROTOCOL),
      0
    }
  }
};

/**
 * Generic UEFI Entrypoint for 'ArmFvpDxe' driver
 * See UEFI specification for the details of the parameters
 */
EFI_STATUS
EFIAPI
ArmFvpInitialise (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  EFI_STATUS                   Status;
  CONST ARM_VEXPRESS_PLATFORM  *Platform;
  BOOLEAN                      NeedFallback;
  UINTN                        TextDevicePathBaseSize;
  UINTN                        TextDevicePathSize;
  CHAR16                       *TextDevicePath;
  VOID                         *Buffer;

  Status = gBS->InstallProtocolInterface (
                  &ImageHandle,
                  &gEfiDevicePathProtocolGuid, EFI_NATIVE_INTERFACE,
                  &mVirtioBlockDevicePath
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = ArmVExpressGetPlatform (&Platform);
  if (!EFI_ERROR (Status)) {
    //
    // In the case of the FVP base and foundation platforms, two default
    // text device paths for the FDT are defined. The first one, like every
    // other platform, ends with a file name that identifies the platform. The
    // second one ends with the fallback file name "fdt.dtb" for historical
    // backward compatibility reasons.
    //
    NeedFallback = (Platform->Id == ARM_FVP_BASE) ||
                   (Platform->Id == ARM_FVP_FOUNDATION);

    TextDevicePathBaseSize = StrSize ((CHAR16*)PcdGetPtr (PcdFvpFdtDevicePathsBase)) - sizeof (CHAR16);
    TextDevicePathSize     = TextDevicePathBaseSize + StrSize (Platform->FdtName);
    if (NeedFallback) {
      TextDevicePathSize += TextDevicePathBaseSize + StrSize (mFdtFallbackName);
    }

    TextDevicePath = AllocatePool (TextDevicePathSize);
    if (TextDevicePath != NULL) {
      StrCpy (TextDevicePath, ((CHAR16*)PcdGetPtr (PcdFvpFdtDevicePathsBase)));
      StrCat (TextDevicePath, Platform->FdtName);

      if (NeedFallback) {
        StrCat (TextDevicePath, L";");
        StrCat (TextDevicePath, ((CHAR16*)PcdGetPtr (PcdFvpFdtDevicePathsBase)));
        StrCat (TextDevicePath, mFdtFallbackName);
      }

      Buffer = PcdSetPtr (PcdFdtDevicePaths, &TextDevicePathSize, TextDevicePath);
      if (Buffer == NULL) {
        DEBUG ((
          EFI_D_ERROR,
          "ArmFvpDxe: Setting of FDT device path in PcdFdtDevicePaths failed - %r\n", EFI_BUFFER_TOO_SMALL
          ));
      }
      FreePool (TextDevicePath);
    } else {
        DEBUG ((
          EFI_D_ERROR,
          "ArmFvpDxe: Setting of FDT device path in PcdFdtDevicePaths failed - %r\n", EFI_OUT_OF_RESOURCES
          ));
    }
  }

  // Declare the Virtio BlockIo device
  Status = VirtioMmioInstallDevice (ARM_FVP_BASE_VIRTIO_BLOCK_BASE, ImageHandle);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "ArmFvpDxe: Failed to install Virtio block device\n"));
  }

  // Install dynamic Shell command to run baremetal binaries.
  Status = ShellDynCmdRunAxfInstall (ImageHandle);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "ArmFvpDxe: Failed to install ShellDynCmdRunAxf\n"));
  }

  return Status;
}
