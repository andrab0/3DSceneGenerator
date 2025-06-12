import os
from pathlib import Path
from PIL import Image
import imageio.v3 as iio

# Optional: OpenEXR fallback if needed
try:
    import OpenEXR
    import Imath
    EXR_SUPPORT = True
except ImportError:
    EXR_SUPPORT = False

# Folder to scan
INPUT_DIR = "../../Models/Textures"  # schimba daca e alta cale

# Formate acceptate ca input
EXTENSIONS = [".jpg", ".jpeg", ".exr"]

def convert_image(file_path: Path):
    output_path = file_path.with_suffix(".png")

    if output_path.exists():
        print(f"[SKIP] {output_path.name} already exists.")
        return

    ext = file_path.suffix.lower()

    try:
        if ext in [".jpg", ".jpeg"]:
            img = Image.open(file_path).convert("RGB")
            img.save(output_path)
            print(f"[OK] Converted {file_path.name} → {output_path.name}")

        elif ext == ".exr" and EXR_SUPPORT:
            exr_file = OpenEXR.InputFile(str(file_path))
            dw = exr_file.header()['dataWindow']
            size = (dw.max.x - dw.min.x + 1, dw.max.y - dw.min.y + 1)

            # Canali: R/G/B sau Y (gray)
            channels = exr_file.header()['channels'].keys()
            if 'R' in channels and 'G' in channels and 'B' in channels:
                pt = Imath.PixelType(Imath.PixelType.FLOAT)
                r = Image.frombytes("F", size, exr_file.channel("R", pt))
                g = Image.frombytes("F", size, exr_file.channel("G", pt))
                b = Image.frombytes("F", size, exr_file.channel("B", pt))

                img = Image.merge("RGB", (r, g, b)).convert("RGB")
            elif 'Y' in channels:
                pt = Imath.PixelType(Imath.PixelType.FLOAT)
                y = Image.frombytes("F", size, exr_file.channel("Y", pt))
                img = y.convert("L").convert("RGB")
            else:
                raise Exception("Unsupported EXR channel layout")

            # Normalize + save
            img = img.point(lambda x: min(255, x * 255))
            img.save(output_path)
            print(f"[OK] Converted {file_path.name} → {output_path.name}")
        elif ext == ".exr":
            print(f"[SKIP] OpenEXR not installed: {file_path.name}")
    except Exception as e:
        print(f"[ERROR] {file_path.name}: {e}")

if __name__ == "__main__":
    convert_image(Path(INPUT_DIR))
