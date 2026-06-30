#!/usr/bin/env python3
import sys
import os
import subprocess
import struct
import tempfile

def convert(input_mp4, output_lraw, width=480, height=270, fps=30, duration=5):
    print(f"Converting {input_mp4} to {output_lraw}...")
    print(f"Target size: {width}x{height}, FPS: {fps}, Max Duration: {duration}s")

    if not os.path.exists(input_mp4):
        print(f"Error: input file not found: {input_mp4}", file=sys.stderr)
        return False

    os.makedirs(os.path.dirname(os.path.abspath(output_lraw)), exist_ok=True)

    with tempfile.NamedTemporaryFile(prefix="mahina-wallpaper-", suffix=".raw", delete=False) as tmp:
        temp_raw = tmp.name

    try:
        # Extract BGRA frames. On little-endian machines, BGRA byte order maps
        # directly to Mahina's ARGB/XRGB 32-bit pixel memory layout.
        vf = (
            f"scale={width}:{height}:force_original_aspect_ratio=increase,"
            f"crop={width}:{height},fps={fps}"
        )
        cmd = [
            "ffmpeg", "-y",
            "-loglevel", "error",
            "-i", input_mp4,
            "-t", str(duration),
            "-vf", vf,
            "-f", "rawvideo",
            "-pix_fmt", "bgra",
            temp_raw
        ]

        try:
            subprocess.run(cmd, check=True)
        except FileNotFoundError:
            print("Error: ffmpeg not found", file=sys.stderr)
            return False
        except subprocess.CalledProcessError as e:
            print(f"Error: ffmpeg failed: {e}", file=sys.stderr)
            return False

        file_size = os.path.getsize(temp_raw)
        frame_size = width * height * 4
        frame_count = file_size // frame_size

        if frame_count == 0:
            print("Error: 0 frames extracted", file=sys.stderr)
            return False

        usable_size = frame_count * frame_size
        print(f"Extracted {frame_count} frames successfully. Writing output LRAW...")

        with open(output_lraw, "wb") as out:
            out.write(b"LRAW")
            out.write(struct.pack("<III", width, height, frame_count))
            with open(temp_raw, "rb") as f:
                remaining = usable_size
                while remaining > 0:
                    chunk = f.read(min(1024 * 1024, remaining))
                    if not chunk:
                        break
                    out.write(chunk)
                    remaining -= len(chunk)

        print("Conversion complete!")
        return True
    finally:
        try:
            os.remove(temp_raw)
        except OSError:
            pass

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: convert_video.py <input.mp4> <output.lraw> [width] [height] [fps] [duration]")
        sys.exit(1)
        
    inp = sys.argv[1]
    out = sys.argv[2]
    w = int(sys.argv[3]) if len(sys.argv) > 3 else 480
    h = int(sys.argv[4]) if len(sys.argv) > 4 else 270
    fps = int(sys.argv[5]) if len(sys.argv) > 5 else 30
    dur = int(sys.argv[6]) if len(sys.argv) > 6 else 5
    
    success = convert(inp, out, w, h, fps, dur)
    sys.exit(0 if success else 1)
