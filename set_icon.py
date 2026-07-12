import os
import ctypes
import ctypes.wintypes
from PIL import Image

# ============================================================
# Step 1: Convert picture.jpg to .ico with multiple sizes
# ============================================================
img_path = r"G:\qtproject\AiChat\picture.jpg"
ico_path = r"G:\qtproject\AiChat\picture.ico"

img = Image.open(img_path)
print(f"Original: {img.size}, mode={img.mode}")

# Convert to RGBA for proper transparency if needed
if img.mode != "RGBA":
    img = img.convert("RGBA")

# Standard icon sizes (Windows uses 256, 128, 64, 48, 32, 16)
icon_sizes = [(256, 256), (128, 128), (64, 64), (48, 48), (32, 32), (16, 16)]

# Create resized versions
resized_images = []
for size in icon_sizes:
    # Use LANCZOS for high quality resizing
    r = img.resize(size, Image.LANCZOS)
    resized_images.append(r)

# Save as .ico with multiple sizes
resized_images[0].save(
    ico_path,
    format="ICO",
    sizes=[(s, s) for s, _ in icon_sizes],
    append_images=resized_images[1:]
)

if os.path.exists(ico_path):
    sz = os.path.getsize(ico_path)
    print(f"ICO created: {ico_path} ({sz} bytes)")
else:
    print("Failed to create ICO")
    exit(1)

# ============================================================
# Step 2: Update desktop shortcut icon
# ============================================================
class GUID(ctypes.Structure):
    _fields_ = [
        ("Data1", ctypes.c_ulong),
        ("Data2", ctypes.c_ushort),
        ("Data3", ctypes.c_ushort),
        ("Data4", ctypes.c_ubyte * 8),
    ]

CLSID_ShellLink = GUID(0x00021401, 0x0000, 0x0000,
    (ctypes.c_ubyte * 8)(0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46))
IID_IPersistFile = GUID(0x0000010b, 0x0000, 0x0000,
    (ctypes.c_ubyte * 8)(0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46))

ole32 = ctypes.windll.ole32
ole32.CoInitialize(None)

# Open existing shortcut via IPersistFile::Load
pPersistFile = ctypes.c_void_p()
hr = ole32.CoCreateInstance(
    ctypes.byref(CLSID_ShellLink), None, 1,  # CLSCTX_INPROC
    ctypes.byref(IID_IPersistFile),
    ctypes.byref(pPersistFile)
)
print(f"CoCreateInstance: 0x{hr & 0xFFFFFFFF:08X}")

pf_vtable_ptr = ctypes.c_void_p.from_address(pPersistFile.value)
pf_vtable = ctypes.cast(pf_vtable_ptr, ctypes.POINTER(ctypes.c_void_p))

# Load shortcut
Load_func = ctypes.WINFUNCTYPE(
    ctypes.HRESULT, ctypes.c_void_p, ctypes.c_wchar_p, ctypes.c_ulong
)(pf_vtable[5])  # IPersistFile::Load at index 5

lnk_path = os.path.join(os.environ["USERPROFILE"], "Desktop", "AiChat.lnk")
hr = Load_func(pPersistFile, lnk_path, 0)
print(f"Load: 0x{hr & 0xFFFFFFFF:08X}")

# QI for IShellLinkW to set icon
IID_IShellLinkW = GUID(0x000214F9, 0x0000, 0x0000,
    (ctypes.c_ubyte * 8)(0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46))

QI_func = ctypes.WINFUNCTYPE(
    ctypes.HRESULT, ctypes.c_void_p, ctypes.POINTER(GUID), ctypes.POINTER(ctypes.c_void_p)
)(pf_vtable[0])

pShellLink = ctypes.c_void_p()
hr = QI_func(pPersistFile, ctypes.byref(IID_IShellLinkW), ctypes.byref(pShellLink))
print(f"QI(IShellLinkW): 0x{hr & 0xFFFFFFFF:08X}")

# SetIconLocation at vtable index 17
sl_vtable_ptr = ctypes.c_void_p.from_address(pShellLink.value)
sl_vtable = ctypes.cast(sl_vtable_ptr, ctypes.POINTER(ctypes.c_void_p))

SetIcon_func = ctypes.WINFUNCTYPE(
    ctypes.HRESULT, ctypes.c_void_p, ctypes.c_wchar_p, ctypes.c_int
)(sl_vtable[17])

hr = SetIcon_func(pShellLink, ico_path, 0)
print(f"SetIconLocation: 0x{hr & 0xFFFFFFFF:08X}")

# Save shortcut
Save_func = ctypes.WINFUNCTYPE(
    ctypes.HRESULT, ctypes.c_void_p, ctypes.c_wchar_p, ctypes.c_int, ctypes.c_ulong
)(pf_vtable[6])  # IPersistFile::Save at index 6

hr = Save_func(pPersistFile, lnk_path, True, 0)
print(f"Save: 0x{hr & 0xFFFFFFFF:08X}")

# Release
Rel_pf = ctypes.WINFUNCTYPE(ctypes.c_ulong, ctypes.c_void_p)(pf_vtable[2])
Rel_pf(pPersistFile)

ole32.CoUninitialize()

if hr == 0:
    print("SUCCESS: Shortcut icon updated!")
else:
    print("FAILED to update shortcut icon")
