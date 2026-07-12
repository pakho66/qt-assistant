import os
import ctypes
import ctypes.wintypes

class GUID(ctypes.Structure):
    _fields_ = [
        ("Data1", ctypes.c_ulong),
        ("Data2", ctypes.c_ushort),
        ("Data3", ctypes.c_ushort),
        ("Data4", ctypes.c_ubyte * 8),
    ]

CLSID_ShellLink = GUID(0x00021401, 0x0000, 0x0000,
    (ctypes.c_ubyte * 8)(0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46))
IID_IShellLinkW = GUID(0x000214F9, 0x0000, 0x0000,
    (ctypes.c_ubyte * 8)(0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46))
IID_IPersistFile = GUID(0x0000010b, 0x0000, 0x0000,
    (ctypes.c_ubyte * 8)(0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46))

ole32 = ctypes.windll.ole32
ole32.CoInitialize(None)

pShellLink = ctypes.c_void_p()
hr = ole32.CoCreateInstance(ctypes.byref(CLSID_ShellLink), None, 1,
    ctypes.byref(IID_IShellLinkW), ctypes.byref(pShellLink))
print(f"CoCreateInstance: 0x{hr & 0xFFFFFFFF:08X}")

vptr = ctypes.c_void_p.from_address(pShellLink.value)
vt = ctypes.cast(vptr, ctypes.POINTER(ctypes.c_void_p))

# SetPath (index 20)
fn = ctypes.WINFUNCTYPE(ctypes.HRESULT, ctypes.c_void_p, ctypes.c_wchar_p)
fn(vt[20])(pShellLink, r"G:\qtproject\AiChat\debug\AiChat.exe")

# SetWorkingDirectory (index 9)
fn(vt[9])(pShellLink, r"G:\qtproject\AiChat\debug")

# SetDescription (index 7)
fn(vt[7])(pShellLink, "AiChat - AI\u7f16\u7a0bAgent")

# SetIconLocation (index 17)
fi = ctypes.WINFUNCTYPE(ctypes.HRESULT, ctypes.c_void_p, ctypes.c_wchar_p, ctypes.c_int)
fi(vt[17])(pShellLink, r"G:\qtproject\AiChat\ai_icon.ico", 0)

# QI IPersistFile to save
pPF = ctypes.c_void_p()
qi = ctypes.WINFUNCTYPE(ctypes.HRESULT, ctypes.c_void_p,
    ctypes.POINTER(GUID), ctypes.POINTER(ctypes.c_void_p))
qi(vt[0])(pShellLink, ctypes.byref(IID_IPersistFile), ctypes.byref(pPF))

pf_v = ctypes.c_void_p.from_address(pPF.value)
pf_t = ctypes.cast(pf_v, ctypes.POINTER(ctypes.c_void_p))

# Save (index 6)
sv = ctypes.WINFUNCTYPE(ctypes.HRESULT, ctypes.c_void_p, ctypes.c_wchar_p, ctypes.c_int, ctypes.c_ulong)
lnk = os.path.join(os.environ["USERPROFILE"], "Desktop", "AiChat.lnk")
hr = sv(pf_t[6])(pPF, lnk, True, 0)
print(f"Save: 0x{hr & 0xFFFFFFFF:08X}  -> {lnk}")

# Release
rel = ctypes.WINFUNCTYPE(ctypes.c_ulong, ctypes.c_void_p)
rel(pf_t[2])(pPF)
rel(vt[2])(pShellLink)
ole32.CoUninitialize()

# Force icon cache refresh
import subprocess
subprocess.run(['ie4uinit.exe', '-show'], capture_output=True)
print("Icon cache refreshed")

if hr == 0:
    print("OK - new shortcut created with custom icon")
else:
    print("FAILED")
