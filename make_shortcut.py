import ctypes
import ctypes.wintypes
import os

# Define GUID structure
class GUID(ctypes.Structure):
    _fields_ = [
        ("Data1", ctypes.c_ulong),
        ("Data2", ctypes.c_ushort),
        ("Data3", ctypes.c_ushort),
        ("Data4", ctypes.c_ubyte * 8),
    ]

# CLSID_ShellLink: {00021401-0000-0000-C000-000000000046}
CLSID_ShellLink = GUID(0x00021401, 0x0000, 0x0000,
                        (ctypes.c_ubyte * 8)(0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46))

# IID_IShellLinkW: {000214F9-0000-0000-C000-000000000046}
IID_IShellLinkW = GUID(0x000214F9, 0x0000, 0x0000,
                        (ctypes.c_ubyte * 8)(0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46))

# IID_IPersistFile: {0000010b-0000-0000-C000-000000000046}
IID_IPersistFile = GUID(0x0000010b, 0x0000, 0x0000,
                         (ctypes.c_ubyte * 8)(0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46))

# CLSCTX_INPROC_SERVER = 1
CLSCTX_INPROC_SERVER = 1

# Initialize COM
ole32 = ctypes.windll.ole32
ole32.CoInitialize(None)

# CoCreateInstance(IShellLinkW)
pShellLink = ctypes.c_void_p()
hr = ole32.CoCreateInstance(
    ctypes.byref(CLSID_ShellLink),
    None,
    CLSCTX_INPROC_SERVER,
    ctypes.byref(IID_IShellLinkW),
    ctypes.byref(pShellLink)
)
print(f"CoCreateInstance HRESULT: 0x{hr & 0xFFFFFFFF:08X}")

if hr != 0:
    print("Failed to create IShellLinkW instance")
    ole32.CoUninitialize()
    exit(1)

# IShellLinkW vtable indices:
# 0: QI, 1: AddRef, 2: Release
# 3: GetPath, 4: GetIDList, 5: SetIDList
# 6: GetDescription, 7: SetDescription
# 8: GetWorkingDirectory, 9: SetWorkingDirectory
# 10: GetArguments, 11: SetArguments
# 12: GetHotkey, 13: SetHotkey
# 14: GetShowCmd, 15: SetShowCmd
# 16: GetIconLocation, 17: SetIconLocation
# 18: SetRelativePath, 19: Resolve, 20: SetPath

# Read vtable pointer
vtable_ptr = ctypes.c_void_p.from_address(pShellLink.value)
vtable = ctypes.cast(vtable_ptr, ctypes.POINTER(ctypes.c_void_p))

# SetPath(LPCWSTR) - index 20
SetPath_func = ctypes.WINFUNCTYPE(
    ctypes.HRESULT, ctypes.c_void_p, ctypes.c_wchar_p
)(vtable[20])

target = r"G:\qtproject\AiChat\debug\AiChat.exe"
hr = SetPath_func(pShellLink, target)
print(f"SetPath HRESULT: 0x{hr & 0xFFFFFFFF:08X}")

# SetWorkingDirectory(LPCWSTR) - index 9
SetWorkDir_func = ctypes.WINFUNCTYPE(
    ctypes.HRESULT, ctypes.c_void_p, ctypes.c_wchar_p
)(vtable[9])

hr = SetWorkDir_func(pShellLink, r"G:\qtproject\AiChat\debug")
print(f"SetWorkingDirectory HRESULT: 0x{hr & 0xFFFFFFFF:08X}")

# SetDescription(LPCWSTR) - index 7
SetDesc_func = ctypes.WINFUNCTYPE(
    ctypes.HRESULT, ctypes.c_void_p, ctypes.c_wchar_p
)(vtable[7])

hr = SetDesc_func(pShellLink, "AiChat - Qt AI 编程 Agent")
print(f"SetDescription HRESULT: 0x{hr & 0xFFFFFFFF:08X}")

# SetIconLocation(LPCWSTR, int) - index 17
SetIcon_func = ctypes.WINFUNCTYPE(
    ctypes.HRESULT, ctypes.c_void_p, ctypes.c_wchar_p, ctypes.c_int
)(vtable[17])

hr = SetIcon_func(pShellLink, target, 0)
print(f"SetIconLocation HRESULT: 0x{hr & 0xFFFFFFFF:08X}")

# QueryInterface for IPersistFile
pPersistFile = ctypes.c_void_p()
QI_func = ctypes.WINFUNCTYPE(
    ctypes.HRESULT, ctypes.c_void_p, ctypes.POINTER(GUID), ctypes.POINTER(ctypes.c_void_p)
)(vtable[0])

hr = QI_func(pShellLink, ctypes.byref(IID_IPersistFile), ctypes.byref(pPersistFile))
print(f"QueryInterface(IPersistFile) HRESULT: 0x{hr & 0xFFFFFFFF:08X}")

if hr != 0:
    print("Failed to get IPersistFile")
    ole32.CoUninitialize()
    exit(1)

# IPersistFile vtable:
# 0: QI, 1: AddRef, 2: Release
# 3: GetClassID, 4: IsDirty, 5: Load, 6: Save, 7: SaveCompleted, 8: GetCurFile

pf_vtable_ptr = ctypes.c_void_p.from_address(pPersistFile.value)
pf_vtable = ctypes.cast(pf_vtable_ptr, ctypes.POINTER(ctypes.c_void_p))

# Save(LPCOLESTR, BOOL, DWORD) - index 6
Save_func = ctypes.WINFUNCTYPE(
    ctypes.HRESULT, ctypes.c_void_p, ctypes.c_wchar_p, ctypes.c_int, ctypes.c_ulong
)(pf_vtable[6])

lnk_path = os.path.join(os.environ["USERPROFILE"], "Desktop", "AiChat.lnk")
hr = Save_func(pPersistFile, lnk_path, True, 0)
print(f"Save HRESULT: 0x{hr & 0xFFFFFFFF:08X}")

if hr == 0:
    print(f"SUCCESS: Shortcut created at {lnk_path}")
else:
    print(f"FAILED to save shortcut")

# Release IPersistFile
Release_pf = ctypes.WINFUNCTYPE(ctypes.c_ulong, ctypes.c_void_p)(pf_vtable[2])
Release_pf(pPersistFile)

# Release IShellLinkW
Release_sl = ctypes.WINFUNCTYPE(ctypes.c_ulong, ctypes.c_void_p)(vtable[2])
Release_sl(pShellLink)

ole32.CoUninitialize()
